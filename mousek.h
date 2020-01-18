#ifndef __KERNEL__
#define __KERNEL__
#endif
#ifndef MODULE
#define MODULE
#endif

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

#define DRIVER_VERSION "v0.1"
#define DRIVER_AUTHOR "Zyurin Max"
#define DRIVER_DESC "Mouse driver"
#define DRIVER_LICENSE "GPL"


#define DEVICE_NAME "mousek"

#define KBD_IRQ 1                       /* IRQ number for keyboard */
#define KBD_DATA_REG 0x60               /* I/O port for keyboard data */
#define KBD_SCANCODE_MASK 0x7f
#define KBD_STATUS_MASK 0x80
#define MOUSE_SENSITIVITY 10            /* Sensitivity in pixels per move */

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

struct mousek_device {
        char name[128];
        char phys[64];
        struct usb_device *usbdev;
        struct input_dev *dev;
        struct urb *irq;

        signed char *data;
        dma_addr_t data_dma;
};

struct mousek_device *mouse;

static void mousek_device_irq(struct urb *urb);
static int mousek_device_open(struct input_dev *dev);
static void mousek_device_close(struct input_dev *dev);
static int mousek_device_probe(struct usb_interface *intf, const struct usb_device_id *id);
static void mousek_device_disconnect(struct usb_interface *intf);
static irqreturn_t keyboard_interceptor(int irq, void *dev_id);


static struct usb_device_id mousek_device_id_table [] = {
        { USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
                USB_INTERFACE_PROTOCOL_MOUSE) },
        { }     /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, mousek_device_id_table);

static struct usb_driver mousek_device_driver = {
        .name           = DEVICE_NAME,
        .probe          = mousek_device_probe,
        .disconnect     = mousek_device_disconnect,
        .id_table       = mousek_device_id_table,
};

module_usb_driver(mousek_device_driver);

