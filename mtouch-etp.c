/*
 * MicroTouch (ETP-PB-031C-0349) serial touchscreen driver
 *
 * Copyright (c) 2016 Jari Vetoniemi
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

/*
 * 2016/09/29 Jari Vetoniemi <mailroxas@gmail.com>
 *   Copied mtouch.c and edited for this particular ETP-PB protocol
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>

#define SERIO_MICROTOUCH_ETP 0x41

#define DRIVER_DESC "MicroTouch (ETP-PB-031C-0349) serial touchscreen driver"

MODULE_AUTHOR("Jari Vetoniemi <mailroxas@gmail.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

enum {
	BEGIN_TOUCH = 0x81,
	END_TOUCH = 0x80,
};

#define ROWS 0x10
#define COLS 0x80
#define MAX_XC (ROWS * COLS)
#define MAX_YC (ROWS * COLS)
#define GET_XC(data) (data[3] * COLS + data[4])
#define GET_YC(data) (data[1] * COLS + data[2])
#define GET_TOUCHED(data) (data[0] == BEGIN_TOUCH)

struct mtouch {
	struct input_dev *dev;
	struct serio *serio;
	int idx;
	unsigned char data[5];
	char phys[32];
};

static void process(struct mtouch *mtouch)
{
	struct input_dev *dev = mtouch->dev;
	input_report_abs(dev, ABS_X, MAX_XC - GET_XC(mtouch->data));
	input_report_abs(dev, ABS_Y, MAX_YC - GET_YC(mtouch->data));
	input_report_key(dev, BTN_TOUCH, GET_TOUCHED(mtouch->data));
	input_sync(dev);
}

static bool valid_type(unsigned char type)
{
	return (type == BEGIN_TOUCH || type == END_TOUCH);
}

static irqreturn_t interrupt(struct serio *serio,
		unsigned char data, unsigned int flags)
{
	struct mtouch* mtouch = serio_get_drvdata(serio);

	mtouch->data[mtouch->idx++] = data;

	if (mtouch->idx >= sizeof(mtouch->data)) {
		if (valid_type(mtouch->data[0])) {
			process(mtouch);
		}
		mtouch->idx = 0;
	}

	return IRQ_HANDLED;
}

static void disconnect(struct serio *serio)
{
	struct mtouch* mtouch = serio_get_drvdata(serio);

	input_get_device(mtouch->dev);
	input_unregister_device(mtouch->dev);
	serio_close(serio);
	serio_set_drvdata(serio, NULL);
	input_put_device(mtouch->dev);
	kfree(mtouch);
}

static int connect(struct serio *serio, struct serio_driver *drv)
{
	struct mtouch *mtouch;
	struct input_dev *input_dev;
	int err;

	mtouch = kzalloc(sizeof(struct mtouch), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!mtouch || !input_dev) {
		err = -ENOMEM;
		goto fail1;
	}

	mtouch->serio = serio;
	mtouch->dev = input_dev;
	snprintf(mtouch->phys, sizeof(mtouch->phys), "%s/input0", serio->phys);

	input_dev->name = "MicroTouch (ETP-PB-031C-0349) Serial TouchScreen";
	input_dev->phys = mtouch->phys;
	input_dev->id.bustype = BUS_RS232;
	input_dev->id.vendor = SERIO_MICROTOUCH_ETP;
	input_dev->id.product = 0;
	input_dev->id.version = 0x0100;
	input_dev->dev.parent = &serio->dev;
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(mtouch->dev, ABS_X, 0, MAX_XC, 0, 0);
	input_set_abs_params(mtouch->dev, ABS_Y, 0, MAX_YC, 0, 0);

	serio_set_drvdata(serio, mtouch);

	err = serio_open(serio, drv);
	if (err)
		goto fail2;

	err = input_register_device(mtouch->dev);
	if (err)
		goto fail3;

	return 0;

 fail3:	serio_close(serio);
 fail2:	serio_set_drvdata(serio, NULL);
 fail1:	input_free_device(input_dev);
	kfree(mtouch);
	return err;
}

static struct serio_device_id serio_ids[] = {
	{
		.type	= SERIO_RS232,
		.proto	= SERIO_MICROTOUCH_ETP,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{ 0 }
};

MODULE_DEVICE_TABLE(serio, serio_ids);

static struct serio_driver mtouch_etp_drv = {
	.driver		= {
		.name	= "mtouch-etp",
	},
	.description	= DRIVER_DESC,
	.id_table	= serio_ids,
	.interrupt	= interrupt,
	.connect	= connect,
	.disconnect	= disconnect,
};

module_serio_driver(mtouch_etp_drv);
