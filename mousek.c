#include "mousek.h"

static irqreturn_t keyboard_interceptor(int irq, void *dev_id)
{
	struct input_dev *dev = mouse->dev;

	char scancode;
	scancode = inb(KBD_DATA_REG);
        printk(KERN_INFO "mousek: Keyboard interrupt handled!\n");

	/* Moving mouse on set number of pixels on arrow key presses */
	if (scancode == 0x48) /* up */
		input_report_rel(dev, REL_Y, -MOUSE_SENSITIVITY);

	if (scancode == 0x50) /* down */
		input_report_rel(dev, REL_Y, MOUSE_SENSITIVITY);

	if (scancode == 0x4B) /* left */
		input_report_rel(dev, REL_X, -MOUSE_SENSITIVITY);

	if (scancode == 0x4D) /* right */
		input_report_rel(dev, REL_X, MOUSE_SENSITIVITY);

	/* Clicking mouse buttons on key presses */
	if (scancode == 0x38)
	{
		input_report_key(dev, BTN_LEFT, 1);
		input_report_key(dev, BTN_LEFT, 0);
	}

	if (scancode == 0x1D)
	{
		input_report_key(dev, BTN_RIGHT, 1);
		input_report_key(dev, BTN_RIGHT, 0);
	}

	input_sync(dev);

	return IRQ_HANDLED;
}

static void mousek_device_irq(struct urb *urb)
{
        mouse = urb->context;
        signed char *data = mouse->data;
        struct input_dev *dev = mouse->dev;
        int status;

        switch (urb->status) {
        case 0:                 
                input_report_key(dev, BTN_LEFT,   data[0] & 0x01);
                input_report_key(dev, BTN_RIGHT,  data[0] & 0x02);
                input_report_key(dev, BTN_MIDDLE, data[0] & 0x04);

                input_report_rel(dev, REL_X,     data[1]);
                input_report_rel(dev, REL_Y,     data[3]);

                input_sync(dev);
                break;
        case -ECONNRESET:       
        case -ENOENT:
        case -ESHUTDOWN:
                return;
        default:               
                break;
        }

        status = usb_submit_urb (urb, GFP_ATOMIC);
        if (status)
                dev_err(&mouse->usbdev->dev,
                        "can't resubmit intr, %s-%s/input0, status %d\n",
                        mouse->usbdev->bus->bus_name,
                        mouse->usbdev->devpath, status);
}

static int mousek_device_open(struct input_dev *dev)
{
        mouse = input_get_drvdata(dev);

        mouse->irq->dev = mouse->usbdev;
        if (usb_submit_urb(mouse->irq, GFP_KERNEL))
                return -EIO;

        return 0;
}

static void mousek_device_close(struct input_dev *dev)
{
        mouse = input_get_drvdata(dev);
        usb_kill_urb(mouse->irq);
}

static int mousek_device_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
        struct usb_device *dev = interface_to_usbdev(intf);
        struct usb_host_interface *interface;
        struct usb_endpoint_descriptor *endpoint;
        struct input_dev *input_dev;
        int pipe, maxp;
        int error = -ENOMEM;
        int retval = 1;

        interface = intf->cur_altsetting;

        if (interface->desc.bNumEndpoints != 1)
                return -ENODEV;

        endpoint = &interface->endpoint[0].desc;
        if (!usb_endpoint_is_int_in(endpoint))
                return -ENODEV;

        pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
        maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

        mouse = kzalloc(sizeof(struct mousek_device), GFP_KERNEL);
        input_dev = input_allocate_device();
        if (!mouse || !input_dev)
        {
                printk(KERN_ERR "mousek: Failed to install device\n");
                input_free_device(input_dev);
                kfree(mouse);
                return error;               
        }

        mouse->data = usb_alloc_coherent(dev, 8, GFP_ATOMIC, &mouse->data_dma);
        if (!mouse->data)
        {
                printk(KERN_ERR "mousek: Failed to install device\n");
                input_free_device(input_dev);
                kfree(mouse);
                return error;               
        }

        mouse->irq = usb_alloc_urb(0, GFP_KERNEL);
        if (!mouse->irq)
        {
                usb_free_coherent(dev, 8, mouse->data, mouse->data_dma);
                input_free_device(input_dev);
                kfree(mouse);
                return error;
        }

        mouse->usbdev = dev;
        mouse->dev = input_dev;

        if (dev->manufacturer)
                strlcpy(mouse->name, dev->manufacturer, sizeof(mouse->name));

        if (dev->product) {
                if (dev->manufacturer)
                        strlcat(mouse->name, " ", sizeof(mouse->name));
                strlcat(mouse->name, dev->product, sizeof(mouse->name));
        }

        if (!strlen(mouse->name))
                snprintf(mouse->name, sizeof(mouse->name),
                         "USB HIDBP Mouse %04x:%04x",
                         le16_to_cpu(dev->descriptor.idVendor),
                         le16_to_cpu(dev->descriptor.idProduct));

        usb_make_path(dev, mouse->phys, sizeof(mouse->phys));
        strlcat(mouse->phys, "/input0", sizeof(mouse->phys));

        input_dev->name = mouse->name;
        input_dev->phys = mouse->phys;
        usb_to_input_id(dev, &input_dev->id);
        input_dev->dev.parent = &intf->dev;

        input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);

	input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_RIGHT) | BIT_MASK(BTN_MIDDLE);
	input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);

        input_set_drvdata(input_dev, mouse);

        input_dev->open = mousek_device_open;
        input_dev->close = mousek_device_close;

        usb_fill_int_urb(mouse->irq, dev, pipe, mouse->data,
                         (maxp > 8 ? 8 : maxp),
                         mousek_device_irq, mouse, endpoint->bInterval);
        mouse->irq->transfer_dma = mouse->data_dma;
        mouse->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

        error = input_register_device(mouse->dev);
        if (error)
        {
                printk(KERN_ERR "mousek: Failed to install device\n");
                usb_free_urb(mouse->irq);
                usb_free_coherent(dev, 8, mouse->data, mouse->data_dma);
                input_free_device(input_dev);
                kfree(mouse);
                return error;
        }

        usb_set_intfdata(intf, mouse);

	retval = request_irq(KBD_IRQ, keyboard_interceptor, IRQF_SHARED, "keyboard_interceptor", (void *)keyboard_interceptor);

	if (retval)
	{
	        printk(KERN_ERR "mousek: Failed to install device\n");
		input_free_device(mouse->dev);
		kfree(mouse);
                usb_free_urb(mouse->irq);
                usb_free_coherent(dev, 8, mouse->data, mouse->data_dma);
                input_free_device(input_dev);
                kfree(mouse);
                return -ENODEV;
	}

        printk(KERN_ERR "mousek: Install successful\n");
        return 0;
}

static void mousek_device_disconnect(struct usb_interface *intf)
{
        mouse = usb_get_intfdata (intf);

        usb_set_intfdata(intf, NULL);
        if (mouse) {
                usb_kill_urb(mouse->irq);
                input_unregister_device(mouse->dev);
                usb_free_urb(mouse->irq);
                usb_free_coherent(interface_to_usbdev(intf), 8, mouse->data, mouse->data_dma);
                kfree(mouse);
                free_irq(KBD_IRQ, (void *)(keyboard_interceptor));
                printk(KERN_ERR "mousek: Uninstall successful\n");
        }
}