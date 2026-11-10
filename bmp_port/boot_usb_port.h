/*
 * boot_usb_port.h
 *
 * Device-controller hooks the board/firmware layer needs from the USB port.
 * The CherryUSB entry points themselves (usb_dc_low_level_init /
 * usb_dc_low_level_deinit) are implemented in port/<chip>/boot_usb_<chip>.c.
 */
#ifndef BOOT_USB_PORT_H
#define BOOT_USB_PORT_H

#include "ch32fun.h"

/*
 * Which USB device controller this chip port drives.  The rest of the firmware
 * only needs these two names: the interrupt vector to mask around a critical
 * section, and the register base to hand to the controller driver.
 */
#if defined(CH32L103)
#define USB_PORT_IRQn       USBFS_IRQn
#define USB_PORT_BASE       USBFS_BASE
#else
#define USB_PORT_IRQn       USBHS_IRQn
#define USB_PORT_BASE       USBHS_BASE
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Bring the USB device controller down before handing over to the
 * application.  Dropping the D+ pull-up is what makes the host re-enumerate. */
void boot_usb_port_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_USB_PORT_H */
