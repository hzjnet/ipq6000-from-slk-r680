#ifndef _USBMISC_H
#define _USBMISC_H

#include <stdio.h>
#include "QMIThread.h"

/* ---------------------------------------------------------------------- */
enum
{
    INVALID_TYPE,
    SOFTWARE_QMI,
    SOFTWARE_MBIM,
    HARDWARE_PCIE,
    HARDWARE_USB,
};

#define USB_CLASS_COMM			2
#define USB_CLASS_VENDOR_SPEC		0xff

// extern int get_driver_type(PROFILE_T *profile);
extern int get_usbmisc_filepath(char *path);
extern int find_matched_device(char **pp_qmichannel, char **pp_usbnet_adapter, int *class, int *usb_busnum, int *usb_devnum);
extern int reset_usb(PROFILE_T *profile);

/* ---------------------------------------------------------------------- */
#endif /* _USBMISC_H */