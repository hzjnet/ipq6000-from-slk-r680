#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <net/if.h>
#include "QMIThread.h"

int iWwanDHCP = 1; // by QMI Commands to obtain the IP address

#define CONVERT_IPV6_NETWORK_ADDRESS_TO_STRING(ipv6_nw_address, ipv6_str)            \
          {                                                                          \
            struct sockaddr_in6             ipv6_addr;                               \
            memcpy(ipv6_addr.sin6_addr.s6_addr, ipv6_nw_address,                     \
                       sizeof(ipv6_addr.sin6_addr.s6_addr));                         \
            inet_ntop(AF_INET6, &(ipv6_addr.sin6_addr), ipv6_str, INET6_ADDRSTRLEN); \
          }

static int system_call(const char *shell_cmd) {
    dbg_time("%s", shell_cmd);
    return system(shell_cmd);
}

static void sc_set_mtu(const char *usbnet_adapter, int ifru_mtu) {
    int inet_sock;
    struct ifreq ifr;

    inet_sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (inet_sock > 0) {
        strcpy(ifr.ifr_name, usbnet_adapter);

        if (!ioctl(inet_sock, SIOCGIFMTU, &ifr)) {
            if (ifr.ifr_ifru.ifru_mtu != ifru_mtu) {
                dbg_time("change mtu %d -> %d", ifr.ifr_ifru.ifru_mtu , ifru_mtu);
                ifr.ifr_ifru.ifru_mtu = ifru_mtu;
                ioctl(inet_sock, SIOCSIFMTU, &ifr);
            }
        }

        close(inet_sock);
    }
}

#define ERROR(fmt, ...) do { dbg_time(fmt, __VA_ARGS__); return; } while(0)
void print_driver_info(const char *ifname)
{
    int socId, fd = -1;
    struct ethtool_drvinfo drvinfo;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    (void) strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    socId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socId < 0) ERROR("Socket failed. Errno = %d Errstr = %s", errno, strerror(errno));

    drvinfo.cmd = ETHTOOL_GDRVINFO;
    ifr.ifr_data = (void *)&drvinfo;

    fd = ioctl(socId, SIOCETHTOOL, &ifr);
    close(socId);

    if (fd < 0) ERROR("Ioctl failed. Errno = %d Errstr = %s", errno, strerror(errno));

    dbg_time("network card driver = %s, driver version = %s", drvinfo.driver, drvinfo.version);
}

void get_driver_rmnet_info(PROFILE_T *profile, RMNET_INFO *rmnet_info) {
    int socId;
    struct ifreq ifr;
    int rc;
    int request = 0x89F3;
    unsigned char data[512];

    memset(rmnet_info, 0x00, sizeof(*rmnet_info));

    socId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socId < 0) ERROR("Socket failed. Errno = %d Errstr = %s", errno, strerror(errno));

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, profile->usbnet_adapter, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    ifr.ifr_ifru.ifru_data = (void *)data;

    rc = ioctl(socId, request, &ifr);
    close(socId);

    if (rc < 0) ERROR("Ioctl failed. Errno = %d Errstr = %s", errno, strerror(errno));

    memcpy(rmnet_info, data, sizeof(*rmnet_info));
}

void set_driver_ul_params_setting(PROFILE_T *profile, QMAP_SETTING *qmap_settings) {
    int ifc_ctl_sock;
    struct ifreq ifr;
    int rc;
    int request = 0x89F2;

    ifc_ctl_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ifc_ctl_sock <= 0) {
        dbg_time("socket() failed: %s\n", strerror(errno));
        return;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, profile->usbnet_adapter, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    ifr.ifr_ifru.ifru_data = (void *)qmap_settings;

    rc = ioctl(ifc_ctl_sock, request, &ifr);
    if (rc < 0) {
        dbg_time("ioctl(0x%x, qmap_settings) failed: %s, rc=%d", request, strerror(errno), rc);
    }

    close(ifc_ctl_sock);
}

void set_driver_link_state(PROFILE_T *profile, int link_state) {
    char link_file[128];
    int fd;
    int new_state = 0;

    snprintf(link_file, sizeof(link_file), "/sys/class/net/%s/link_state", profile->usbnet_adapter);
    fd = open(link_file, O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd == -1) {
        if (errno != ENOENT)
            dbg_time("Fail to access %s, errno: %d (%s)", link_file, errno, strerror(errno));
        return;
    }

    if (profile->qmap_mode <= 1) {
        new_state = !!link_state;
    }
    else {
        //0x80 means link off this pdp
        new_state = (link_state ? 0x00 : 0x80) + profile->pdp;
    }

    snprintf(link_file, sizeof(link_file), "%d\n", new_state);
    write(fd, link_file, sizeof(link_file));

    if (link_state == 0 && profile->qmapnet_adapter && strcmp(profile->qmapnet_adapter, profile->usbnet_adapter)) {
        size_t rc;

        lseek(fd, 0, SEEK_SET);
        rc = read(fd, link_file, sizeof(link_file));
        if (rc > 1 && (!strncasecmp(link_file, "0\n", 2) || !strncasecmp(link_file, "0x0\n", 4))) {
            snprintf(link_file, sizeof(link_file), "ifconfig %s down", profile->usbnet_adapter);
            system_call(link_file);
        }
    }

    close(fd);
}

static void ifc_init_ifr(const char *name, struct ifreq *ifr)
{
    memset(ifr, 0, sizeof(struct ifreq));
    strncpy(ifr->ifr_name, name, IFNAMSIZ);
    ifr->ifr_name[IFNAMSIZ - 1] = 0;
}

static short ifc_get_flags(const char *ifname)
{
    int inet_sock;
    struct ifreq ifr;
    int ret = 0;

    inet_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (inet_sock > 0) {
        ifc_init_ifr(ifname, &ifr);

        if (!ioctl(inet_sock, SIOCGIFFLAGS, &ifr)) {
            ret = ifr.ifr_ifru.ifru_flags;
        }

        close(inet_sock);
    }

    return ret;
}

void update_resolv_conf(int iptype, const char *ifname, const char *dns1, const char *dns2) {
    const char *dns_file = "/etc/resolv.conf";
    FILE *dns_fp;
    char dns_line[256];
    #define MAX_DNS 16
    char *dns_info[MAX_DNS];
    char dns_tag[64];
    int dns_match = 0;
    int i;

    snprintf(dns_tag, sizeof(dns_tag), "# IPV%d %s", iptype, ifname);

    for (i = 0; i < MAX_DNS; i++)
        dns_info[i] = NULL;

    dns_fp = fopen(dns_file, "r");
    if (dns_fp) {
        i = 0;
        dns_line[sizeof(dns_line)-1] = '\0';

        while((fgets(dns_line, sizeof(dns_line)-1, dns_fp)) != NULL) {
            if ((strlen(dns_line) > 1) && (dns_line[strlen(dns_line) - 1] == '\n'))
                dns_line[strlen(dns_line) - 1] = '\0';
            if (strstr(dns_line, dns_tag)) {
                dns_match++;
                continue;
            }
            dns_info[i++] = strdup(dns_line);
            if (i == MAX_DNS)
                break;
        }

        fclose(dns_fp);
    }
    else if (errno != ENOENT) {
        dbg_time("fopen %s fail, errno:%d (%s)", dns_file, errno, strerror(errno));
        return;
    }

    if (dns1 == NULL && dns_match == 0)
        return;

    dns_fp = fopen(dns_file, "w");
    if (dns_fp) {
        if (dns1)
            fprintf(dns_fp, "nameserver %s %s\n", dns1, dns_tag);
        if (dns2)
            fprintf(dns_fp, "nameserver %s %s\n", dns2, dns_tag);

        for (i = 0; i < MAX_DNS && dns_info[i]; i++)
            fprintf(dns_fp, "%s\n", dns_info[i]);
        fclose(dns_fp);
    }
    else {
        dbg_time("fopen %s fail, errno:%d (%s)", dns_file, errno, strerror(errno));
    }

    for (i = 0; i < MAX_DNS && dns_info[i]; i++)
        free(dns_info[i]);
}

static int set_rawip_data_format(const char *ifname) {
    int fd;
    char raw_ip[128];
    char shell_cmd[128];
    char mode[2] = "N";
    int mode_change = 0;

    snprintf(raw_ip, sizeof(raw_ip), "/sys/class/net/%s/qmi/raw_ip", ifname);
    if (access(raw_ip, F_OK))
        return 0;

    fd = open(raw_ip, O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) {
        dbg_time("Warning:%s fail to open(%s), errno:%d (%s)",  __LINE__, raw_ip, errno, strerror(errno));
        return 0;
    }

    read(fd, mode, 2);
    if (mode[0] == '0' || mode[0] == 'N') {
        snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s down", ifname);
        system_call(shell_cmd);
        dbg_time("echo Y > /sys/class/net/%s/qmi/raw_ip", ifname);
        mode[0] = 'Y';
        write(fd, mode, 2);
        mode_change = 1;
        snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s up", ifname);
        system_call(shell_cmd);
    }

    close(fd);
    return mode_change;
}

static void* udhcpc_thread_function(void* arg) {
    FILE * udhcpc_fp;
    char *udhcpc_cmd = (char *)arg;
	return NULL;
    if (udhcpc_cmd == NULL)
        return NULL;

    dbg_time("%s", udhcpc_cmd);
    udhcpc_fp = popen(udhcpc_cmd, "r");
    free(udhcpc_cmd);
    if (udhcpc_fp) {
        char buf[0xff];

        while((fgets(buf, sizeof(buf), udhcpc_fp)) != NULL) {
            if ((strlen(buf) > 1) && (buf[strlen(buf) - 1] == '\n'))
                buf[strlen(buf) - 1] = '\0';
            dbg_time("%s", buf);
        }

        pclose(udhcpc_fp);
    }

    return NULL;
}

//#define USE_DHCLIENT
#ifdef USE_DHCLIENT
static int dhclient_alive = 0;
#endif
static int dibbler_client_alive = 0;

void udhcpc_start(PROFILE_T *profile) {
    char *ifname = profile->usbnet_adapter;
    char shell_cmd[256];

    set_driver_link_state(profile, 1);

    if (profile->qmapnet_adapter) {
        ifname = profile->qmapnet_adapter;
    }
    if (profile->rawIP && profile->ipv4.Address && profile->ipv4.Mtu) {
        sc_set_mtu(ifname, (profile->ipv4.Mtu));
    }

    if (strcmp(ifname, profile->usbnet_adapter)) {
        snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s up", profile->usbnet_adapter);
        system_call(shell_cmd);
        if (ifc_get_flags(ifname)&IFF_UP) {
            snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s down", ifname);
            system_call(shell_cmd);
        }
    }

    snprintf(shell_cmd, sizeof(shell_cmd) - 1, "ifconfig %s up", ifname);
    system_call(shell_cmd);

#if 1 //for bridge mode, only one public IP, so donot run udhcpc to obtain
    {
        const char *BRIDGE_MODE_FILE = "/sys/module/GobiNet/parameters/bridge_mode";
        const char *BRIDGE_IPV4_FILE = "/sys/module/GobiNet/parameters/bridge_ipv4";

        if (strncmp(qmichannel, "/dev/qcqmi", strlen("/dev/qcqmi"))) {
            BRIDGE_MODE_FILE = "/sys/module/qmi_wwan/parameters/bridge_mode";
            BRIDGE_IPV4_FILE = "/sys/module/qmi_wwan/parameters/bridge_ipv4";
        }

        if (profile->ipv4.Address && !access(BRIDGE_MODE_FILE, R_OK)) {
            int bridge_fd = open(BRIDGE_MODE_FILE, O_RDONLY);
            char bridge_mode[2] = {0, 0};

            if (bridge_fd > 0) {
                read(bridge_fd, &bridge_mode, sizeof(bridge_mode));
                close(bridge_fd);
                if(bridge_mode[0] != '0') {
                    snprintf(shell_cmd, sizeof(shell_cmd), "echo 0x%08x > %s", profile->ipv4.Address, BRIDGE_IPV4_FILE);
                    system_call(shell_cmd);
                    return;
                }
            }
        }
    }
#endif

//because must use udhcpc to obtain IP when working on ETH mode,
//so it is better also use udhcpc to obtain IP when working on IP mode.
//use the same policy for all modules
    if (iWwanDHCP > 0 && profile->rawIP != 0) //mdm9x07/
    {
        if (profile->ipv4.Address) {
            unsigned char *ip = (unsigned char *)&profile->ipv4.Address;
            unsigned char *gw = (unsigned char *)&profile->ipv4.Gateway;
            unsigned char *netmask = (unsigned char *)&profile->ipv4.SubnetMask;
            unsigned char *dns1 = (unsigned char *)&profile->ipv4.DnsPrimary;
            unsigned char *dns2 = (unsigned char *)&profile->ipv4.DnsSecondary;

            snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s %d.%d.%d.%d netmask %d.%d.%d.%d",ifname,
                ip[3], ip[2], ip[1], ip[0], netmask[3], netmask[2], netmask[1], netmask[0]);
            system_call(shell_cmd);

            //Resetting default routes
            snprintf(shell_cmd, sizeof(shell_cmd), "route del default gw 0.0.0.0 dev %s", ifname);
            while(!system(shell_cmd));

            /* snprintf(shell_cmd, sizeof(shell_cmd), "route add default gw %d.%d.%d.%d dev %s metric 0", gw[3], gw[2], gw[1], gw[0], ifname);
            system_call(shell_cmd); */

            //Adding DNS
            // if (profile->ipv4.DnsSecondary == 0)
            //     profile->ipv4.DnsSecondary = profile->ipv4.DnsPrimary;
            if (strlen((char *)dns1)) {
                memset(shell_cmd, 0, sizeof(shell_cmd));
                snprintf(shell_cmd, sizeof(shell_cmd), "echo 'nameserver %d.%d.%d.%d' > /etc/resolv.conf",
                         dns1[3], dns1[2], dns1[1], dns1[0]);
                system_call(shell_cmd);
            }
            if (strlen((char *)dns2)) {
                memset(shell_cmd, 0, sizeof(shell_cmd));
                snprintf(shell_cmd, sizeof(shell_cmd), "echo 'nameserver %d.%d.%d.%d' >> /etc/resolv.conf",
                         dns2[3], dns2[2], dns2[1], dns2[0]);
                system_call(shell_cmd);
            }
        }

        if (profile->ipv6.Address[0] && profile->ipv6.PrefixLengthIPAddr) {
            unsigned char *ip = (unsigned char *)profile->ipv6.Address;
#if 0
            snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s inet6 add %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x/%u",
                ifname, ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15], profile->ipv6.PrefixLengthIPAddr);
#else
            snprintf(shell_cmd, sizeof(shell_cmd),
                "ip -6 addr add %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x/%u dev %s",
                ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15],profile->ipv6.PrefixLengthIPAddr, ifname);
#endif
            system_call(shell_cmd);
            snprintf(shell_cmd, sizeof(shell_cmd), "route -A inet6 add default dev %s", ifname);
            system_call(shell_cmd);

            if (profile->ipv6.DnsPrimary[0] || profile->ipv6.DnsSecondary[0]) {
                char dns1[INET6_ADDRSTRLEN];
                char dns2[INET6_ADDRSTRLEN];

                if (profile->ipv6.DnsPrimary[0])
                {
                    memset(dns1, 0, sizeof(dns1));
                    memset(shell_cmd, 0, sizeof(shell_cmd));
                    CONVERT_IPV6_NETWORK_ADDRESS_TO_STRING(profile->ipv6.DnsPrimary, dns1);
                    if (profile->isIpv4) {
                        snprintf(shell_cmd, sizeof(shell_cmd), "echo 'nameserver %s' >> /etc/resolv.conf",
                             dns1);
                    }else {
                        snprintf(shell_cmd, sizeof(shell_cmd), "echo 'nameserver %s' > /etc/resolv.conf",
                             dns1);
                    }

                    system_call(shell_cmd);
                }

                if (profile->ipv6.DnsSecondary[0])
                {
                    memset(dns2, 0, sizeof(dns2));
                    memset(shell_cmd, 0, sizeof(shell_cmd));
                    CONVERT_IPV6_NETWORK_ADDRESS_TO_STRING(profile->ipv6.DnsSecondary, dns2);
                    snprintf(shell_cmd, sizeof(shell_cmd), "echo 'nameserver %s' >> /etc/resolv.conf",
                             dns2);
                    system_call(shell_cmd);
                }
            }
        }
        return;
    }

    {
        char udhcpc_cmd[128];
        pthread_attr_t udhcpc_thread_attr;
        pthread_t udhcpc_thread_id;

        pthread_attr_init(&udhcpc_thread_attr);
        pthread_attr_setdetachstate(&udhcpc_thread_attr, PTHREAD_CREATE_DETACHED);

        if (profile->ipv4.Address) {
#ifdef USE_DHCLIENT
            snprintf(udhcpc_cmd, sizeof(udhcpc_cmd), "dhclient -4 -d --no-pid %s", ifname);
            dhclient_alive++;
#else
            dbg_time("[Warning]: udhcpc depend default.script, "
                "may be located under '/etc/udhcpc' or '/usr/share/udhcpc/'.\n");

            if (access("/etc/udhcpc/default.script", X_OK)) {
                dbg_time("No found executable default.script in /etc/udhcpc/");
            }

            if (access("/usr/share/udhcpc/default.script", X_OK)) {
                dbg_time("No found executable default.script in /usr/share/udhcpc/");
            }

            //-f,--foreground    Run in foreground
            //-b,--background    Background if lease is not obtained
            //-n,--now        Exit if lease is not obtained
            //-q,--quit        Exit after obtaining lease
            //-t,--retries N        Send up to N discover packets (default 3)
            //-s,--script PROG	Run PROG at DHCP events (default /etc/udhcpc/default.script)
            snprintf(udhcpc_cmd, sizeof(udhcpc_cmd), "busybox udhcpc -f -n -q -t 5 -i %s", ifname);
#endif

#ifdef USE_DHCLIENT
            pthread_create(&udhcpc_thread_id, &udhcpc_thread_attr, udhcpc_thread_function, (void*)strdup(udhcpc_cmd));
            sleep(1);
#else
            if (profile->rawIP != 0)
            {
                // set_rawip_data_format(ifname);
            }
            pthread_create(&udhcpc_thread_id, NULL, udhcpc_thread_function, (void*)strdup(udhcpc_cmd));
            pthread_join(udhcpc_thread_id, NULL);
#endif
        }

        if (profile->ipv6.Address[0] && profile->ipv6.PrefixLengthIPAddr) {
#ifdef USE_DHCLIENT
            snprintf(udhcpc_cmd, sizeof(udhcpc_cmd), "dhclient -6 -d --no-pid %s",  ifname);
            dhclient_alive++;
#else
            /*
                DHCPv6: Dibbler - a portable DHCPv6
                1. download from http://klub.com.pl/dhcpv6/
                2. cross-compile
                    2.1 ./configure --host=arm-linux-gnueabihf
                    2.2 copy dibbler-client to your board
                3. mkdir -p /var/log/dibbler/ /var/lib/ on your board
                4. create /etc/dibbler/client.conf on your board, the content is
                    log-mode short
                    log-level 7
                    iface wwan0 {
                        ia
                        option dns-server
                    }
                 5. run "dibbler-client start" to get ipV6 address
                 6. run "route -A inet6 add default dev wwan0" to add default route
            */
            snprintf(shell_cmd, sizeof(shell_cmd), "route -A inet6 add default %s", ifname);
            system_call(shell_cmd);
            snprintf(udhcpc_cmd, sizeof(udhcpc_cmd), "dibbler-client run");
            dibbler_client_alive++;
#endif

            pthread_create(&udhcpc_thread_id, &udhcpc_thread_attr, udhcpc_thread_function, (void*)strdup(udhcpc_cmd));
        }
    }
}

void udhcpc_stop(PROFILE_T *profile) {
    char *ifname = profile->usbnet_adapter;
    char shell_cmd[128];

    set_driver_link_state(profile, 0);

    if (profile->qmapnet_adapter) {
        ifname = profile->qmapnet_adapter;
    }

#ifdef USE_DHCLIENT
    if (dhclient_alive) {
        system("killall dhclient");
        dhclient_alive = 0;
    }
#endif
    if (dibbler_client_alive) {
        system("killall dibbler-client");
        dibbler_client_alive = 0;
    }
    snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s 0.0.0.0", ifname);
    system_call(shell_cmd);
    snprintf(shell_cmd, sizeof(shell_cmd), "ifconfig %s down", ifname);
    system_call(shell_cmd);
}
