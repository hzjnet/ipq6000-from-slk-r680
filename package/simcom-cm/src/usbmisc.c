#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/usbdevice_fs.h>
#include "usbmisc.h"


#define MY_PATH_MAX 256

#define USB_CLASS_COMM				2
#define USB_CLASS_VENDOR_SPEC		0xff
#define SIMCOM_VENDOR_ID				0x1e0e

#define SYSFS_INTu(de, tgt, name) do { tgt.name = read_sysfs_file_int(de,#name,10); } while(0)
#define SYSFS_INTx(de, tgt, name) do { tgt.name = read_sysfs_file_int(de,#name,16); } while(0)

typedef struct usb_ifc_info usb_ifc_info;

struct usb_ifc_info
{
    unsigned short idVendor;
    unsigned short idProduct;
    unsigned char devnum;  /* Device address */
    unsigned short busnum; /* Bus number */
    unsigned int bcdDevice;
    unsigned int bInterfaceClass;
};

static const char sys_bus_usb_devices[] = "/sys/bus/usb/devices";
static unsigned int read_sysfs_file_int(const char *d_name, const char *file, int base)
{
	char buf[12], path[MY_PATH_MAX];
	int fd;
	ssize_t r;
	unsigned long ret;
    snprintf(path, MY_PATH_MAX, "%s/%s/%s", sys_bus_usb_devices, d_name, file);
	path[MY_PATH_MAX - 1] = '\0';
	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto error;
	memset(buf, 0, sizeof(buf));
	r = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (r < 0)
		goto error;
	buf[sizeof(buf) - 1] = '\0';
	ret = strtoul(buf, NULL, base);
	return (unsigned int)ret;

      error:
	perror(path);
	return 0;
}

static int read_sysfs_string(const char *sysfs_name, const char *sysfs_node,
                             char* buf, int bufsize)
{
    char path[80];
    int fd, n;

    snprintf(path, sizeof(path),
             "/sys/bus/usb/devices/%s/%s", sysfs_name, sysfs_node);
    path[sizeof(path) - 1] = '\0';

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    n = read(fd, buf, bufsize - 1);
    close(fd);

    if (n < 0)
        return -1;

    buf[n] = '\0';

    return n;
}

static int read_sysfs_number(const char *sysfs_name, const char *sysfs_node)
{
    char buf[16];
    int value;

    if (read_sysfs_string(sysfs_name, sysfs_node, buf, sizeof(buf)) < 0)
        return -1;

    if (sscanf(buf, "%d", &value) != 1)
        return -1;

    return value;
}

int get_usbmisc_filepath(char *path)
{
    size_t len = strlen(path);

    if (!access(path, R_OK))
    {
        path[len] = '\0';
        strcat(path, "/GobiQMI");
        if (!access(path, R_OK))
            return 0;

        path[len] = '\0';
        strcat(path, "/usbmisc");
        if (!access(path, R_OK))
            return 0;

        path[len] = '\0';
        strcat(path, "/usb");
        if (!access(path, R_OK))
            return 0;
    }

    return -1;
}

static int charsplit(const char *src,char* desc,int n,const char* splitStr){
    char* p;
    char*p1;
    int len;

    len=strlen(splitStr);
    p=strstr(src,splitStr);
    if(p==NULL)
        return -1;
    p1=strstr(p,"\n");
    if(p1==NULL)
        return -1;
    memset(desc,0,n);
    memcpy(desc,p+len,p1-p-len);

    return 0;
}

static int get_dev_major_minor(char* path, int *major, int *minor){
    int fd = -1;
    char desc[128] = {0};
    char devmajor[64],devminor[64];
    int n = 0;
    if(access(path, R_OK | W_OK)){
        return 1;
    }
    if((fd = open(path, O_RDWR)) < 0){
        return 1;
    }
    n = read(fd , desc, sizeof(desc));
    if(n == sizeof(desc)) {
        dbg_time("may be overflow");
    }
    close(fd);
    if(charsplit(desc,devmajor,64,"MAJOR=")==-1
        ||charsplit(desc,devminor,64,"MINOR=")==-1 ) {
        return 2;
    }
    *major = atoi(devmajor);
    *minor = atoi(devminor);
    return 0;
}

/* True if name isn't a valid name for a USB device in /sys/bus/usb/devices.
 * Device names are made up of numbers, dots, and dashes, e.g., '7-1.5'.
 * We reject interfaces (e.g., '7-1.5:1.0') and host controllers (e.g. 'usb1').
 * The name must also start with a digit, to disallow '.' and '..'
 */
static int badname(const char *name)
{
    if (!isdigit(*name))
      return 1;
    while(*++name) {
        if(!isdigit(*name) && *name != '.' && *name != '-')
            return 1;
    }
    return 0;
}

int dir_get_child(const char *dirname, char *buff, unsigned bufsize)
{
    struct dirent *de = NULL;
    DIR *dirptr = opendir(dirname);
    if (!dirptr)
        goto error;
    while ((de = readdir(dirptr))) {
        if (de->d_name[0] == '.')
            continue;
        snprintf(buff, bufsize, "%s", de->d_name);
        break;
    }

    closedir(dirptr);
    return 0;
error:
    buff[0] = '\0';
    if (dirptr) closedir(dirptr);
    return -1;
}

int find_matched_device(char **pp_qmichannel, char **pp_usbnet_adapter, int *class, int *usb_busnum, int *usb_devnum) {
    struct dirent *de;
    DIR *busdir;
    struct usb_ifc_info info;
    char subdir[255];
    char netcard[32+1] = {'\0'};
    char qmifile[32+1] = {'\0'};
    int major = 0, minor = 0;

    busdir = opendir(sys_bus_usb_devices);
    if(busdir == NULL) return 0;

    while ((de = readdir(busdir))) {
        DIR *psubDir;
        struct dirent* subent = NULL;

        if(badname(de->d_name)) continue;

        SYSFS_INTx(de->d_name, info, idProduct);
        SYSFS_INTx(de->d_name, info, idVendor);
        SYSFS_INTu(de->d_name, info, busnum);
        SYSFS_INTu(de->d_name, info, devnum);
        SYSFS_INTx(de->d_name, info, bcdDevice);

        if (info.idVendor != SIMCOM_VENDOR_ID) continue;
        dbg_time("Find idProduct=%x idVendor=%x bcdDevice=%4x busnum=%03d devnum=%03d", info.idProduct, info.idVendor, info.bcdDevice, info.busnum, info.devnum);

        if (info.idProduct == 0x9001) {
            snprintf(subdir, sizeof(subdir), "%s/%s:1.5/net", sys_bus_usb_devices, de->d_name);
        }else if (info.idProduct == 0x901e) {
            snprintf(subdir, sizeof(subdir), "%s/%s:1.2/net", sys_bus_usb_devices, de->d_name);
        }else {
            continue;
        }
        dir_get_child(subdir, netcard, sizeof(netcard));
        dbg_time("netdir=%s netcard=%s",subdir, netcard);

        if (netcard[0] == '\0')
            continue;

        if (*pp_usbnet_adapter && strcmp(*pp_usbnet_adapter, netcard))
            continue;

        subdir[strlen(subdir) - strlen("/net")] = '\0';
        if (get_usbmisc_filepath(subdir))
            continue;

        psubDir = opendir(subdir);
        if (psubDir == NULL)
        {
            dbg_time("Cannot open directory: %s, errno: %d (%s)", psubDir, errno, strerror(errno));
            continue;
        }

        while ((subent = readdir(psubDir)) != NULL)
        {
            if (subent->d_name[0] == '.')
                continue;
            snprintf(qmifile, sizeof(qmifile), "/dev/%s", subent->d_name);

            if (access(qmifile, R_OK | F_OK) && errno == ENOENT)
            {
                int ret;

                dbg_time("%s access failed, errno: %d (%s)", qmifile, errno, strerror(errno));
                //get major minor
                char subdir2[255 * 2];
                snprintf(subdir2, sizeof(subdir), "%s/%s/uevent", subdir, subent->d_name);
                if (!get_dev_major_minor(subdir2, &major, &minor))
                {
                    ret = mknod(qmifile, S_IFCHR | 0666, (((major & 0xfff) << 8) | (minor & 0xff) | ((minor & 0xfff00) << 12)));
                    if (ret)
                        dbg_time("please mknod %s c %d %d", qmifile, major, minor);
                }
                else
                {
                    dbg_time("get %s major and minor failed\n", qmifile);
                }
            }
            break;
        }

        closedir(psubDir);

        if (*pp_usbnet_adapter && strcmp(*pp_usbnet_adapter, netcard))
        {
            continue;
        }
        if (*pp_qmichannel && strcmp(*pp_qmichannel, qmifile))
        {
            continue;
        }

        if (info.idProduct == 0x9001) {
            snprintf(subdir, sizeof(subdir), "%s:1.5", de->d_name);
        }else{
            snprintf(subdir, sizeof(subdir), "%s:1.2", de->d_name);
        }
        SYSFS_INTx(subdir, info, bInterfaceClass);
        
        *class = info.bInterfaceClass;

        if (netcard[0] && qmifile[0])
        {
            *pp_qmichannel = strdup(qmifile);
            *pp_usbnet_adapter = strdup(netcard);
            *usb_busnum = info.busnum;
            *usb_devnum = info.devnum;
            dbg_time("Find qmichannel = %s", qmifile);
            dbg_time("Find usbnet_adapter = %s", netcard);
            closedir(busdir);
            return 1;
        }
    }

    closedir(busdir);

    return (*pp_qmichannel && *pp_usbnet_adapter);

    return 1;
}

int reset_usb(PROFILE_T *profile)
{
    int fd, rc;
    char devpath[128] = {'\0'};
    snprintf(devpath, sizeof(devpath), "/dev/bus/usb/%03d/%03d", profile->usb_busnum, profile->usb_devnum);
    fd = open(devpath, O_WRONLY);
    if (fd < 0)
    {
        dbg_time("%s fail to open %s", __func__, devpath);
        return -1;
    }

    rc = ioctl(fd, USBDEVFS_RESET, 0);
    if (rc < 0)
    {
        dbg_time("Error in ioctl USBDEVFS_RESET");
        return -1;
    }
    dbg_time("Reset %s successful\n",devpath);
    close(fd);
    return 0;
}