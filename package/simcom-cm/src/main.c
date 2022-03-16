#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <dirent.h>

#include "QMIThread.h"
#include "usbmisc.h"
//#define CONFIG_EXIT_WHEN_DIAL_FAILED
//#define CONFIG_BACKGROUND_WHEN_GET_IP
//#define CONFIG_PID_FILE_FORMAT "/var/run/simcom-cm-%s.pid" //for example /var/run/simcom-cm-wwan0.pid

int debug_qmi = 0;
int main_loop = 0;
char * qmichannel;
int qmidevice_control_fd[2];
static int signal_control_fd[2];
#define PROGRAM_VERSION "V20200909_008"

typedef enum {
    CM_RET_OK = 0,
    CM_RET_ERROR,
    CM_RET_MAX
} cm_ret_code;

static size_t safe_fread(const char *filename, void *buf, size_t size) {
    FILE *fp = fopen(filename , "r");
    size_t n = 0;

    memset(buf, 0x00, size);

    if (fp) {
        n = fread(buf, 1, size, fp);
        if (n <= 0 || n == size) {
            dbg_time("warnning: fail to fread(%s), fread=%zu, buf_size=%zu: (%s)", filename, n, size, strerror(errno));
        }
        fclose(fp);
    }

    return n > 0 ? n : 0;
}

static int start_qmi_qmap_mode_detect(PROFILE_T *profile) {
    char buf[128];
    int n;
    char qmap_netcard[128];
    struct {
        char filename[255 * 2];
        char linkname[255 * 2];
    } *pl;

    pl = (typeof(pl)) malloc(sizeof(*pl));

    if (profile->qmapnet_adapter) {
        free(profile->qmapnet_adapter);;
        profile->qmapnet_adapter = NULL;
    }

    snprintf(pl->linkname, sizeof(pl->linkname), "/sys/class/net/%s/device/driver", profile->usbnet_adapter);
    n = readlink(pl->linkname, pl->filename, sizeof(pl->filename));
    pl->filename[n] = '\0';
    while (pl->filename[n] != '/')
        n--;

    get_driver_rmnet_info(profile, &profile->rmnet_info);

    if (profile->rmnet_info.size) {
        profile->qmap_mode = profile->rmnet_info.qmap_mode;
        if (profile->qmap_mode) {
            int offset_id = profile->pdp - 1;

            if (profile->qmap_mode == 1)
                offset_id = 0;
            profile->muxid = profile->rmnet_info.mux_id[offset_id];
            profile->qmapnet_adapter = strdup( profile->rmnet_info.ifname[offset_id]);
            profile->qmap_size = profile->rmnet_info.rx_urb_size;
            profile->qmap_version = profile->rmnet_info.qmap_version;
        }
        goto _out;
    }

    if (strncmp(profile->qmichannel, "/dev/cdc-wdm", strlen("/dev/cdc-wdm")) == 0) {
        snprintf(pl->filename, sizeof(pl->filename), "/sys/class/net/qmimux%d", profile->pdp - 1);
        if (access(pl->filename, R_OK)) {
            if (errno != ENOENT) {
                dbg_time("fail to access %s, errno: %d (%s)", pl->filename, errno, strerror(errno));
            }
            goto _out;
        }

        //upstream Kernel Style QMAP qmi_wwan.c
        snprintf(pl->filename, sizeof(pl->filename), "/sys/class/net/%s/qmi/add_mux", profile->usbnet_adapter);
        n = safe_fread(pl->filename, buf, sizeof(buf));
        if (n >= 5) {
            dbg_time("Use QMAP by /sys/class/net/%s/qmi/add_mux", profile->usbnet_adapter);

            profile->qmap_mode = n/5; //0x11\n0x22\n0x33\n
            if (profile->qmap_mode > 1) {
                //PDN-X map to qmimux-X
                profile->muxid = (buf[5*(profile->pdp - 1) + 2] - '0')*16 + (buf[5*(profile->pdp - 1) + 3] - '0');
                sprintf(qmap_netcard, "qmimux%d", profile->pdp - 1);
                profile->qmapnet_adapter = strdup(qmap_netcard);
            } else if (profile->qmap_mode == 1) {
                profile->muxid = (buf[5*0 + 2] - '0')*16 + (buf[5*0 + 3] - '0');
                sprintf(qmap_netcard, "qmimux%d", 0);
                profile->qmapnet_adapter = strdup(qmap_netcard);
            }
        }
    }

_out:
    if (profile->qmap_mode) {
        if (profile->qmap_size == 0) {
            profile->qmap_size = 32*1024;
            snprintf(pl->filename, sizeof(pl->filename), "/sys/class/net/%s/qmap_size", profile->usbnet_adapter);
            if (!access(pl->filename, R_OK)) {
                size_t n;
                char buf[32];
                n = safe_fread(pl->filename, buf, sizeof(buf));
                if (n > 0) {
                    profile->qmap_size = atoi(buf);
                }
            }
        }

        if (profile->qmap_version == 0) {
            profile->qmap_version = WDA_UL_DATA_AGG_QMAP_ENABLED_V01;
        }

        dbg_time("qmap_mode = %d, qmap_version = %d, qmap_size = %d, muxid = 0x%02x, qmap_netcard = %s",
            profile->qmap_mode, profile->qmap_version, profile->qmap_size, profile->muxid, profile->qmapnet_adapter);
    }
    free(pl);
    return 0;
}

#ifdef CONFIG_BACKGROUND_WHEN_GET_IP
static int daemon_pipe_fd[2];

static void sc_prepare_daemon(void) {
    pid_t daemon_child_pid;

    if (pipe(daemon_pipe_fd) < 0) {
        dbg_time("%s Faild to create daemon_pipe_fd: %d (%s)", __func__, errno, strerror(errno));
        return;
    }

    daemon_child_pid = fork();
    if (daemon_child_pid > 0) {
        struct pollfd pollfds[] = {{daemon_pipe_fd[0], POLLIN, 0}, {0, POLLIN, 0}};
        int ne, ret, nevents = sizeof(pollfds)/sizeof(pollfds[0]);
        int signo;

        //dbg_time("father");

        close(daemon_pipe_fd[1]);

        if (socketpair( AF_LOCAL, SOCK_STREAM, 0, signal_control_fd) < 0 ) {
            dbg_time("%s Faild to create main_control_fd: %d (%s)", __func__, errno, strerror(errno));
            return;
        }

        pollfds[1].fd = signal_control_fd[1];

        while (1) {
            do {
                ret = poll(pollfds, nevents, -1);
            } while ((ret < 0) && (errno == EINTR));

            if (ret < 0) {
                dbg_time("%s poll=%d, errno: %d (%s)", __func__, ret, errno, strerror(errno));
                goto __daemon_quit;
            }

            for (ne = 0; ne < nevents; ne++) {
                int fd = pollfds[ne].fd;
                short revents = pollfds[ne].revents;

                if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    //dbg_time("%s poll err/hup", __func__);
                    //dbg_time("poll fd = %d, events = 0x%04x", fd, revents);
                    if (revents & POLLHUP)
                        goto __daemon_quit;
                }

                if ((revents & POLLIN) &&  read(fd, &signo, sizeof(signo)) == sizeof(signo)) {
                    if (signal_control_fd[1] == fd) {
                        if (signo == SIGCHLD) {
                            int status;
                            int pid = waitpid(daemon_child_pid, &status, 0);
                            dbg_time("waitpid pid=%d, status=%x", pid, status);
                            goto __daemon_quit;
                        } else {
                            kill(daemon_child_pid, signo);
                        }
                    } else if (daemon_pipe_fd[0] == fd) {
                        //dbg_time("daemon_pipe_signo = %d", signo);
                        goto __daemon_quit;
                    }
                }
            }
        }
__daemon_quit:
        //dbg_time("father exit");
        _exit(0);
    } else if (daemon_child_pid == 0) {
        close(daemon_pipe_fd[0]);
        //dbg_time("child", getpid());
    } else {
        close(daemon_pipe_fd[0]);
        close(daemon_pipe_fd[1]);
        dbg_time("%s Faild to create daemon_child_pid: %d (%s)", __func__, errno, strerror(errno));
    }
}

static void sc_enter_daemon(int signo) {
    if (daemon_pipe_fd[1] > 0)
        if (signo) {
            write(daemon_pipe_fd[1], &signo, sizeof(signo));
            sleep(1);
        }
        close(daemon_pipe_fd[1]);
        daemon_pipe_fd[1] = -1;
        setsid();
    }
#endif

//UINT ifc_get_addr(const char *ifname);
static int s_link = -1;
static void usbnet_link_change(int link, PROFILE_T *profile) {
    if (s_link == link)
        return;

    s_link = link;
    if (link) {
        udhcpc_start(profile);
    } else {
        udhcpc_stop(profile);
    }

#ifdef CONFIG_BACKGROUND_WHEN_GET_IP
    if (link && daemon_pipe_fd[1] > 0) {
        int timeout = 6;
        while (timeout-- /*&& ifc_get_addr(profile->usbnet_adapter) == 0*/) {
            sleep(1);
        }
        sc_enter_daemon(SIGUSR1);
    }
#endif
}

static int check_ipv4_address(PROFILE_T *now_profile) {
    PROFILE_T new_profile_v;
    PROFILE_T *new_profile = &new_profile_v;

    memcpy(new_profile, now_profile, sizeof(PROFILE_T));
    if (requestGetIPAddress(new_profile, IpFamilyV4) == 0) {
         if (new_profile->ipv4.Address != now_profile->ipv4.Address || debug_qmi) {
             unsigned char *l = (unsigned char *)&now_profile->ipv4.Address;
             unsigned char *r = (unsigned char *)&new_profile->ipv4.Address;
             dbg_time("localIP: %d.%d.%d.%d VS remoteIP: %d.%d.%d.%d",
                     l[3], l[2], l[1], l[0], r[3], r[2], r[1], r[0]);
        }
        return (new_profile->ipv4.Address == now_profile->ipv4.Address);
    }
    return 0;
}

static void main_send_event_to_qmidevice(int triger_event) {
     write(qmidevice_control_fd[0], &triger_event, sizeof(triger_event));
}

static void send_signo_to_main(int signo) {
     write(signal_control_fd[0], &signo, sizeof(signo));
}

void qmidevice_send_event_to_main(int triger_event) {
     write(qmidevice_control_fd[1], &triger_event, sizeof(triger_event));
}

#define MAX_PATH 256

static int ls_dir(const char *dir, int (*match)(const char *dir, const char *file, void *argv[]), void *argv[]){
    DIR *pDir;
    struct dirent* ent = NULL;
    int match_times = 0;

    pDir = opendir(dir);
    if (pDir == NULL)  {
        dbg_time("Cannot open directory: %s, errno: %d (%s)", dir, errno, strerror(errno));
        return 0;
    }

    while ((ent = readdir(pDir)) != NULL)  {
        match_times += match(dir, ent->d_name, argv);
    }
    closedir(pDir);

    return match_times;
}

static int is_same_linkfile(const char *dir, const char *file,  void *argv[]){
    const char *qmichannel = (const char *)argv[1];
    char linkname[MAX_PATH];
    char filename[MAX_PATH];
    int linksize;

    snprintf(linkname, MAX_PATH, "%s/%s", dir, file);
    linksize = readlink(linkname, filename, MAX_PATH);
    if (linksize <= 0)
        return 0;

    filename[linksize] = 0;
    if (strcmp(filename, qmichannel))
        return 0;

    dbg_time("%s -> %s", linkname, filename);
    return 1;
}

static int is_brother_process(const char *dir, const char *file, void *argv[]){
    //const char *myself = (const char *)argv[0];
    char linkname[MAX_PATH];
    char filename[MAX_PATH];
    int linksize;
    int i = 0, kill_timeout = 15;
    pid_t pid;

    while (file[i]) {
        if (!isdigit(file[i]))
            break;
        i++;
    }

    if (file[i]) {
        return 0;
    }

    snprintf(linkname, MAX_PATH, "%s/%s/exe", dir, file);
    linksize = readlink(linkname, filename, MAX_PATH);
    if (linksize <= 0)
        return 0;

    filename[linksize] = 0;

    pid = atoi(file);
    if (pid == getpid())
        return 0;

    snprintf(linkname, MAX_PATH, "%s/%s/fd", dir, file);
    if (!ls_dir(linkname, is_same_linkfile, argv))
        return 0;

    dbg_time("%s/%s/exe -> %s", dir, file, filename);
    while (kill_timeout-- && !kill(pid, 0))
    {
        kill(pid, SIGTERM);
        sleep(1);
    }
    if (!kill(pid, 0))
    {
        dbg_time("force kill %s/%s/exe -> %s", dir, file, filename);
        kill(pid, SIGKILL);
        sleep(1);
    }

    return 1;
}

static int kill_brothers(const char *qmichannel){
    char myself[MAX_PATH];
    int filenamesize;
    void *argv[2] = {myself, (void *)qmichannel};

    filenamesize = readlink("/proc/self/exe", myself, MAX_PATH);
    if (filenamesize <= 0)
        return 0;
    myself[filenamesize] = 0;

    if (ls_dir("/proc", is_brother_process, argv))
        sleep(1);

    return 0;
}

static void sc_sigaction(int signo) {
     if (SIGCHLD == signo)
         waitpid(-1, NULL, WNOHANG);
     else if (SIGALRM == signo)
         send_signo_to_main(SIGUSR1);
     else
     {
        if (SIGTERM == signo || SIGHUP == signo || SIGINT == signo)
            main_loop = 0;
        send_signo_to_main(signo);
        main_send_event_to_qmidevice(signo); //main may be wating qmi response
    }
}

pthread_t gQmiThreadID;

static const char *helptext =
"Usage: simcom-cm [options]\n\n"
"Examples:\n"
"Start network connection no parameter\n"
"\tsimcom-cm\n"
"Start network connection with PDP apn\n"
"\tsimcom-cm -s CMNET\n"
"Start network connection with PDP Context ID(Check the ID by the CGDCONT read command) and PDP apn\n"
"\tsimcom-cm -n 6 -s CMNET\n"
"Start network connection with PDP apn and auth\n"
"\tsimcom-cm -s 3gnet test 1234 0 -p 1234 -f log.txt\n\n"
"Options:\n"
"-s, [apn [user password auth]]   Set apn/user/password/auth get from your network provider\n"
"-f, logfilename                  Save log message of this program to file\n"
"-d, device path                  Specify device path(like /dev/cdc-wdm0)\n"
"-i, network interface            Specify network interface(default auto-detect\n"
"-n, Use connection profile       The range of permitted values (minimum value = 1) is returned by the test form of the CGDCONT command\n"
"-4, Set ip-family                IPv4 protocol\n"
"-6, Set ip-family                IPv6 protocol\n"
"-r, reset usb                    reset usb open device.\n"
"-h, help                         display this help text\n\n";

static int usage(const char *progname){
    (void)progname;
    printf("%s", helptext);
    return 0;
}

int qmi_main(PROFILE_T *profile)
{
    int triger_event = 0;
    int signo;
    int qmistatus = 0;
    UCHAR PSAttachedState;
    UCHAR IPv4ConnectionStatus = 0xff; //unknow state
    UCHAR IPv6ConnectionStatus = 0xff; //unknow state
#ifdef CONFIG_SIM
    SIM_Status SIMStatus;
#endif
    char * save_usbnet_adapter = NULL;

    signal(SIGUSR1, sc_sigaction);
    signal(SIGUSR2, sc_sigaction);
    signal(SIGINT,  sc_sigaction);
    signal(SIGTERM, sc_sigaction);
    signal(SIGHUP,  sc_sigaction);
    signal(SIGCHLD, sc_sigaction);
    signal(SIGALRM, sc_sigaction);

#ifdef CONFIG_BACKGROUND_WHEN_GET_IP
    sc_prepare_daemon();
#endif

    if (socketpair( AF_LOCAL, SOCK_STREAM, 0, signal_control_fd) < 0 ) {
        dbg_time("%s Faild to create main_control_fd: %d (%s)", __func__, errno, strerror(errno));
        return -1;
    }

    if ( socketpair( AF_LOCAL, SOCK_STREAM, 0, qmidevice_control_fd ) < 0 ) {
        dbg_time("%s Failed to create thread control socket pair: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

__main_loop:
    if (profile->reset_usb)
    {
        dbg_time("Waiting reset usb");
        if (!reset_usb(profile))
            sleep(1);
    }

    while (!profile->qmichannel || !profile->usbnet_adapter)
    {
        if (find_matched_device(&profile->qmichannel, &profile->usbnet_adapter, &profile->bInterfaceClass, &profile->usb_busnum, &profile->usb_devnum))
            break;
        if (!(profile->qmichannel)) {
            dbg_time("profile->qmichannel");
        }
        if (!(profile->usbnet_adapter)) {
            dbg_time("profile->usbnet_adapter");
        }

        if (main_loop)
        {
            int wait_for_device = 3000;
            dbg_time("Wait for simcom modules connect");
            while (wait_for_device && main_loop) {
                wait_for_device -= 100;
                usleep(100*1000);
            }
            continue;
        }
        dbg_time("Cannot find qmichannel(%s) usbnet_adapter(%s) for simcom modules", profile->qmichannel, profile->usbnet_adapter);
        return -ENODEV;
    }

    print_driver_info(profile->usbnet_adapter);

    if (access(profile->qmichannel, R_OK | W_OK)) {
        dbg_time("Fail to access %s, errno: %d (%s)", profile->qmichannel, errno, strerror(errno));
        return errno;
    }

    start_qmi_qmap_mode_detect(profile);

    if (profile->qmap_mode <= 1)
        kill_brothers(profile->qmichannel);

    qmichannel = profile->qmichannel;
    if (!strncmp(profile->qmichannel, "/dev/qcqmi", strlen("/dev/qcqmi")))
    {
        if (pthread_create( &gQmiThreadID, 0, GobiNetThread, (void *)profile) != 0)
        {
            dbg_time("%s Failed to create GobiNetThread: %d (%s)", __func__, errno, strerror(errno));
            return 0;
        }
    }
    else
    {
        if (pthread_create( &gQmiThreadID, 0, QmiWwanThread, (void *)profile) != 0)
        {
            dbg_time("%s Failed to create QmiWwanThread: %d (%s)", __func__, errno, strerror(errno));
            return 0;
        }
    }

    if ((read(qmidevice_control_fd[0], &triger_event, sizeof(triger_event)) != sizeof(triger_event))
        || (triger_event != RIL_INDICATE_DEVICE_CONNECTED)) {
        dbg_time("%s Failed to init QMIThread: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

    if (!strncmp(profile->qmichannel, "/dev/cdc-wdm", strlen("/dev/cdc-wdm"))) {
        if (QmiWwanInit(profile)) {
            dbg_time("%s Failed to QmiWwanInit: %d (%s)", __func__, errno, strerror(errno));
            return 0;
        }
    }

#ifdef CONFIG_VERSION
    requestBaseBandVersion(NULL);
#endif
    requestSetEthMode(profile);
    if (profile->loopback_state) {
        requestSetLoopBackState(profile->loopback_state, profile->replication_factor);
        profile->loopback_state = 0;
    }

#ifdef CONFIG_SIM
    requestGetSIMStatus(&SIMStatus);
    if ((SIMStatus == SIM_PIN) && profile->pincode) {
        requestEnterSimPin(profile->pincode);
    }
#ifdef CONFIG_IMSI_ICCID
    if (SIMStatus == SIM_READY) {
        requestGetICCID();
        requestGetIMSI();
   }
#endif
#endif
#ifdef CONFIG_APN
    if (profile->apn || profile->user || profile->password) {
        requestSetProfile(profile);
    }
    requestGetProfile(profile);
#endif

    requestRegistrationState(&PSAttachedState);
    send_signo_to_main(SIGUSR2);

#ifdef CONFIG_PID_FILE_FORMAT
    {
        char cmd[255];
        sprintf(cmd, "echo %d > " CONFIG_PID_FILE_FORMAT, getpid(), profile.usbnet_adapter);
        system(cmd);
    }
#endif

    while (1)
    {
        struct pollfd pollfds[] = {{signal_control_fd[1], POLLIN, 0}, {qmidevice_control_fd[0], POLLIN, 0}};
        int ne, ret, nevents = sizeof(pollfds)/sizeof(pollfds[0]);

        do {
            ret = poll(pollfds, nevents,  15*1000);
        } while ((ret < 0) && (errno == EINTR));

        if (ret == 0)
        {
            send_signo_to_main(SIGUSR2);
            continue;
        }

        if (ret <= 0) {
            dbg_time("%s poll=%d, errno: %d (%s)", __func__, ret, errno, strerror(errno));
            goto __main_quit;
        }

        for (ne = 0; ne < nevents; ne++) {
            int fd = pollfds[ne].fd;
            short revents = pollfds[ne].revents;

            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                dbg_time("%s poll err/hup", __func__);
                dbg_time("epoll fd = %d, events = 0x%04x", fd, revents);
                main_send_event_to_qmidevice(RIL_REQUEST_QUIT);
                if (revents & POLLHUP)
                    goto __main_quit;
            }

            if ((revents & POLLIN) == 0)
                continue;

            if (fd == signal_control_fd[1])
            {
                if (read(fd, &signo, sizeof(signo)) == sizeof(signo))
                {
                    alarm(0);
                    switch (signo)
                    {
                        case SIGUSR1: /* start wds*/
                            // dbg_time("start wds\n");
                            if (PSAttachedState != 1 && profile->loopback_state == 0)
                                break;
                            if (PSAttachedState != 1){
                                if(profile->loopback_state == 0)
                                    break;
                            }

                            if (profile->isIpv4 && IPv4ConnectionStatus !=  QWDS_PKT_DATA_CONNECTED){
                                // dbg_time("start datacall");
                                qmistatus = requestSetupDataCall(profile, IpFamilyV4);
                                if (!qmistatus) {
                                    qmistatus = requestGetIPAddress(profile, IpFamilyV4);
                                }
                            }

                            if (profile->isIpv6 && IPv6ConnectionStatus !=  QWDS_PKT_DATA_CONNECTED) {
                                qmistatus = requestSetupDataCall(profile, IpFamilyV6);

                                if (!qmistatus) {
                                    qmistatus = requestGetIPAddress(profile, IpFamilyV6);
                                }
                            }

                            if ((profile->isIpv4 && IPv4ConnectionStatus !=  QWDS_PKT_DATA_CONNECTED) || (profile->isIpv6 && IPv6ConnectionStatus !=  QWDS_PKT_DATA_CONNECTED)){
#ifdef CONFIG_EXIT_WHEN_DIAL_FAILED
                                    kill(getpid(), SIGTERM);
#endif
#ifdef CONFIG_RESET_RADIO
                                    gettimeofday(&nowTime, (struct timezone *) NULL);
                                    if (abs(nowTime.tv_sec - resetRadioTime.tv_sec) > CONFIG_RESET_RADIO) {
                                        resetRadioTime = nowTime;
                                        requestSetOperatingMode(0x06); //same as AT+CFUN=0
                                        requestSetOperatingMode(0x00); //same as AT+CFUN=1
                                    }
#endif
                                    alarm(5); //try to setup data call 5 seconds later
                            }
                        break;

                        case SIGUSR2: /* check net*/
                            // dbg_time("check wds\n");
                            if (profile->isIpv4)
                            {
                                requestQueryDataCall(&IPv4ConnectionStatus, IpFamilyV4);
                                //local ip is different with remote ip
                                if (QWDS_PKT_DATA_CONNECTED == IPv4ConnectionStatus && check_ipv4_address(profile) == 0) {
                                    dbg_time("IP address changed by network");
                                    requestDeactivateDefaultPDP(profile, IpFamilyV4);
                                    IPv4ConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
                                }
                            }else
                            {
                                IPv4ConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
                            }

                            if (profile->isIpv6) {
                                requestQueryDataCall(&IPv6ConnectionStatus, IpFamilyV6);
                            }
                            else {
                                IPv6ConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
                            }

                            if (IPv4ConnectionStatus == QWDS_PKT_DATA_DISCONNECTED && IPv6ConnectionStatus == QWDS_PKT_DATA_DISCONNECTED){
                                usbnet_link_change(0, profile);
                            }
                            else if (IPv4ConnectionStatus == QWDS_PKT_DATA_CONNECTED || IPv6ConnectionStatus == QWDS_PKT_DATA_CONNECTED){
                                usbnet_link_change(1, profile);
                            }

                            if ((profile->isIpv4 && IPv4ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)
                                || (profile->isIpv6 && IPv6ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)) {
                                send_signo_to_main(SIGUSR1);
                            }
                        break;

                        case SIGTERM: /* stop wds*/
                        case SIGHUP:
                        case SIGINT:
                            // dbg_time("stop wds\n");
                            if (profile->isIpv4 && IPv4ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED) {
                                requestDeactivateDefaultPDP(profile, IpFamilyV4);
                            }
                            if (profile->isIpv6 && IPv6ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED) {
                                    requestDeactivateDefaultPDP(profile, IpFamilyV6);
                            }
                            usbnet_link_change(0, profile);
                            if (!strncmp(profile->qmichannel, "/dev/cdc-wdm", strlen("/dev/cdc-wdm")))
                                QmiWwanDeInit();
                            main_send_event_to_qmidevice(RIL_REQUEST_QUIT);
                            goto __main_quit;
                        break;

                        default:
                        break;
                    }
                }
            }

            if (fd == qmidevice_control_fd[0]) {
                if (read(fd, &triger_event, sizeof(triger_event)) == sizeof(triger_event)) {
                    switch (triger_event) {
                        case RIL_INDICATE_DEVICE_DISCONNECTED:
                            // dbg_time("RIL_INDICATE_DEVICE_DISCONNECTED\n");
                            usbnet_link_change(0, profile);
                            if (main_loop)
                            {
                                if (pthread_join(gQmiThreadID, NULL)) {
                                    dbg_time("%s Error joining to listener thread (%s)", __func__, strerror(errno));
                                }
                                profile->qmichannel = NULL;
                                profile->usbnet_adapter = save_usbnet_adapter;
                                goto __main_loop;
                            }
                            goto __main_quit;
                        break;

                        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED: /* Maybe AT+CGACT=0 */
                            // dbg_time("RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED\n");
                            requestRegistrationState(&PSAttachedState);
                            if (PSAttachedState == 1){
                                if ((profile->isIpv4 && IPv4ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)
                                    || (profile->isIpv6 && IPv6ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)) {
                                    send_signo_to_main(SIGUSR1);
                                }
                            }
                        break;

                        case RIL_UNSOL_DATA_CALL_LIST_CHANGED: /* Datacall state changed*/
                        {
                            // dbg_time("RIL_UNSOL_DATA_CALL_LIST_CHANGED\n");
                                    send_signo_to_main(SIGUSR2);
                        }
                        break;

                        case RIL_UNSOL_LOOPBACK_CONFIG_IND:
                        {
                            profile->loopback_state = (profile->loopback_state ? 0 : 1);
                            send_signo_to_main(SIGUSR1);
                        }
                        break;

                        default:
                        break;
                    }
                }
            }
        }
    }

__main_quit:
    usbnet_link_change(0, profile);
    if (pthread_join(gQmiThreadID, NULL)) {
        dbg_time("%s Error joining to listener thread (%s)", __func__, strerror(errno));
    }
    close(signal_control_fd[0]);
    close(signal_control_fd[1]);
    close(qmidevice_control_fd[0]);
    close(qmidevice_control_fd[1]);
    dbg_time("SIMCOM_CM exit...");
    if (logfilefp)
        fclose(logfilefp);

#ifdef CONFIG_PID_FILE_FORMAT
    {
        char cmd[255];
        sprintf(cmd, "rm  " CONFIG_PID_FILE_FORMAT, profile.usbnet_adapter);
        system(cmd);
    }
#endif

    return 0;
}

#define has_more_argv() ((opt < argc) && (argv[opt][0] != '-'))
int main(int argc, char *argv[])
{
    int opt = 1;
    int result = CM_RET_OK;
    char * save_usbnet_adapter = NULL;

    PROFILE_T profile;
#ifdef CONFIG_RESET_RADIO
    struct timeval resetRadioTime = {0};
    struct timeval  nowTime;
    gettimeofday(&resetRadioTime, (struct timezone *) NULL);
#endif

    dbg_time("Build Version: %s", PROGRAM_VERSION);
    memset(&profile, 0x00, sizeof(profile));
    profile.pdp = CONFIG_DEFAULT_PDP;

    if (!strcmp(argv[argc-1], "&"))
        argc--;

    opt = 1;
    while  (opt < argc) {
        if (argv[opt][0] != '-')
            return usage(argv[0]);

        switch (argv[opt++][1])
        {
            case 's':
                profile.apn = profile.user = profile.password = "";
                if (has_more_argv())
                    profile.apn = argv[opt++];
                if (has_more_argv())
                    profile.user = argv[opt++];
                if (has_more_argv())
                {
                    profile.password = argv[opt++];
                    if (profile.password && profile.password[0])
                        profile.auth = 2; //default chap, customers may miss auth
                }
                if (has_more_argv())
                    profile.auth = argv[opt++][0] - '0';
            break;

            case 'm':
                if (has_more_argv())
                    profile.muxid = argv[opt++][0] - '0';
                break;

            case 'p':
                if (has_more_argv())
                    profile.pincode = argv[opt++];
            break;

            case 'n':
                if (has_more_argv())
                    profile.pdp = argv[opt++][0] - '0';
            break;

            case 'f':
                if (has_more_argv())
                {
                    const char * filename = argv[opt++];
                    logfilefp = fopen(filename, "a+");
                    if (!logfilefp) {
                        dbg_time("Fail to open %s, errno: %d(%s)", filename, errno, strerror(errno));
                     }
                }
            break;

            case 'd':
                if (has_more_argv())
                    profile.qmichannel = argv[opt++];
            break;

            case 'i':
                if (has_more_argv())
                    profile.usbnet_adapter = save_usbnet_adapter = argv[opt++];
            break;

            case 'v':
                debug_qmi = 1;
            break;

            case 'l':
                if (has_more_argv())
                {
                    profile.replication_factor = atoi(argv[opt++]);
                    if (profile.replication_factor > 0)
                        profile.loopback_state = 1;
                }
                else
                    main_loop = 1;
                break;
            case '4':
                profile.isIpv4 = 1; //support ipv4
            break;

            case '6':
                profile.isIpv6 = 1; //support ipv6
            break;

            case 'r':
                profile.reset_usb = 1;
            break;

            default:
                return usage(argv[0]);
            break;
        }
    }

    if (profile.isIpv4 != 1 && profile.isIpv6 != 1) { // default enable IPv4
        profile.isIpv4 = 1;
    }

    dbg_time("%s profile[%d] = %s/%s/%s/%d, pincode = %s", argv[0], profile.pdp, profile.apn, profile.user, profile.password, profile.auth, profile.pincode);

    if (!(profile.qmichannel) || !(profile.usbnet_adapter)) {
        if (find_matched_device(&profile.qmichannel, &profile.usbnet_adapter, &profile.bInterfaceClass, &profile.usb_busnum, &profile.usb_devnum))
        {
            profile.hardware_interface = HARDWARE_USB;
        }
        else {
            dbg_time("find_matched_device failed");
            goto error;
        }

        if (profile.hardware_interface == HARDWARE_USB)
        {
            if (profile.bInterfaceClass == USB_CLASS_VENDOR_SPEC)
                profile.software_interface = SOFTWARE_QMI;
            if (profile.bInterfaceClass == USB_CLASS_COMM)
                profile.software_interface = SOFTWARE_MBIM;
        }
    }

    if (profile.software_interface == SOFTWARE_MBIM)
    {
        dbg_time("Modem works in MBIM mode");
        result = mbim_main(&profile);
    }
    else if (profile.software_interface == SOFTWARE_QMI)
    {
        dbg_time("Modem works in QMI mode");
        result = qmi_main(&profile);
    }

error:    

    return result;
}
