/*
 * drivers/input/touchscreen/sweep2sleep.c
 *
 * Copyright (c) 2013, Dennis Rassmann <showp1984@gmail.com>
 * Copyright (c) 2015, Tom G. <roboter972@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>

#define DRIVER_AUTHOR "Dennis Rassmann <showp1984@gmail.com>"
#define DRIVER_DESCRIPTION "Sweep2sleep for almost any device"

#define S2S_PWRKEY_DUR          	60
#define DEFAULT_S2S_Y_MAX               1920
#define DEFAULT_S2S_Y_LIMIT             DEFAULT_S2S_Y_MAX-130
#define DEFAULT_S2S_X_MAX		1080

/* Sweep2sleep right to left */
#define DEFAULT_S2S_X_B0		200
#define DEFAULT_S2S_X_B1		DEFAULT_S2S_X_B0 + 250
#define DEFAULT_S2S_X_B2		DEFAULT_S2S_X_B0 + 550

/* Sweep2sleep left to right */
#define DEFAULT_S2S_X_B3		DEFAULT_S2S_X_B0 + 150
#define DEFAULT_S2S_X_B4		DEFAULT_S2S_X_MAX - 450
#define DEFAULT_S2S_X_B5		DEFAULT_S2S_X_MAX - DEFAULT_S2S_X_B0

static DEFINE_MUTEX(pwrkeyworklock);

/* Sweep2sleep switch */
static unsigned int s2s_enabled = 1;

static int touch_x = 0, touch_y = 0;
static bool touch_x_called = false, touch_y_called = false;
static bool exec_count = true;
static bool scr_on_touch = false, barrier[2] = {false, false};
static bool reverse_barrier[2] = {false, false};

static struct input_dev *sweep2sleep_pwrdev;
static struct workqueue_struct *s2s_input_wq;
static struct work_struct s2s_input_work;
static struct kobject *sweep2sleep_kobj;

static void sweep2sleep_presspwr(struct work_struct *sweep2sleep_presspwr_work)
{
	if (!mutex_trylock(&pwrkeyworklock))
                return;

	input_event(sweep2sleep_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(sweep2sleep_pwrdev, EV_SYN, 0, 0);
	msleep(S2S_PWRKEY_DUR);
	input_event(sweep2sleep_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(sweep2sleep_pwrdev, EV_SYN, 0, 0);
	msleep(S2S_PWRKEY_DUR);

        mutex_unlock(&pwrkeyworklock);
}
static DECLARE_WORK(sweep2sleep_presspwr_work, sweep2sleep_presspwr);

static void sweep2sleep_pwrswitch(void)
{
	schedule_work(&sweep2sleep_presspwr_work);
}

static void sweep2sleep_reset(void)
{
	exec_count = true;
	barrier[0] = false;
	barrier[1] = false;
	reverse_barrier[0] = false;
	reverse_barrier[1] = false;
	scr_on_touch = false;
}

static void detect_sweep2sleep(int sweep_coord, int sweep_height, bool st)
{
	int prev_coord = 0, next_coord = 0;
	int reverse_prev_coord = 0, reverse_next_coord = 0;
	bool single_touch = st;

		/* s2s: right->left */
	if ((single_touch) && (s2s_enabled)) {
		scr_on_touch = true;
		prev_coord = DEFAULT_S2S_X_B5;
		next_coord = DEFAULT_S2S_X_B2;
		if ((barrier[0] == true) ||
			((sweep_coord < prev_coord) &&
			(sweep_coord > next_coord) &&
			(sweep_height > DEFAULT_S2S_Y_LIMIT))) {
			prev_coord = next_coord;
			next_coord = DEFAULT_S2S_X_B1;
			barrier[0] = true;
			if ((barrier[1] == true) ||
				((sweep_coord < prev_coord) &&
				(sweep_coord > next_coord) &&
				(sweep_height > DEFAULT_S2S_Y_LIMIT))) {
				prev_coord = next_coord;
				barrier[1] = true;
				if ((sweep_coord < prev_coord) &&
					(sweep_height > DEFAULT_S2S_Y_LIMIT)) {
					if (sweep_coord < DEFAULT_S2S_X_B0) {
						if (exec_count) {
							sweep2sleep_pwrswitch();
							exec_count = false;
						}
					}
				}
			}
		}
		/* s2s: left->right */
		reverse_prev_coord = DEFAULT_S2S_X_B0;
		reverse_next_coord = DEFAULT_S2S_X_B3;
		if ((reverse_barrier[0] == true) ||
			((sweep_coord > reverse_prev_coord) &&
			(sweep_coord < reverse_next_coord) &&
			(sweep_height > DEFAULT_S2S_Y_LIMIT))) {
			reverse_prev_coord = reverse_next_coord;
			reverse_next_coord = DEFAULT_S2S_X_B4;
			reverse_barrier[0] = true;
			if ((reverse_barrier[1] == true) ||
				((sweep_coord > reverse_prev_coord) &&
				(sweep_coord < reverse_next_coord) &&
				(sweep_height > DEFAULT_S2S_Y_LIMIT))) {
				reverse_prev_coord = reverse_next_coord;
				reverse_barrier[1] = true;
				if ((sweep_coord > reverse_prev_coord) &&
					(sweep_height > DEFAULT_S2S_Y_LIMIT)) {
					if (sweep_coord > DEFAULT_S2S_X_B5) {
						if (exec_count) {
							sweep2sleep_pwrswitch();
							exec_count = false;
						}
					}
				}
			}
		}
	}
}

static void s2s_input_callback(struct work_struct *unused)
{

	detect_sweep2sleep(touch_x, touch_y, true);
}

static void s2s_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value)
{
	if (code == ABS_MT_SLOT) {
		sweep2sleep_reset();
		return;
	}

	if (code == ABS_MT_TRACKING_ID && value == -1) {
		sweep2sleep_reset();
		return;
	}

	if (code == ABS_MT_POSITION_X) {
		touch_x = value;
		touch_x_called = true;
	}

	if (code == ABS_MT_POSITION_Y) {
		touch_y = value;
		touch_y_called = true;
	}

	if (touch_x_called && touch_y_called) {
		touch_x_called = false;
		touch_y_called = false;
		queue_work_on(0, s2s_input_wq, &s2s_input_work);
	}
}

static int input_dev_filter(struct input_dev *dev) {
	if ((strstr(dev->name, "touch")) || (strstr(dev->name, "max1187x")))
		return 0;
	else
		return 1;
}

static int s2s_input_connect(struct input_handler *handler,
				struct input_dev *dev,
				const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	if (input_dev_filter(dev))
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "s2s";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;

err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void s2s_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id s2s_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler s2s_input_handler = {
	.event		= s2s_input_event,
	.connect	= s2s_input_connect,
	.disconnect	= s2s_input_disconnect,
	.name		= "s2s_inputreq",
	.id_table	= s2s_ids,
};

static ssize_t sweep2sleep_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	snprintf(buf, PAGE_SIZE, "%u", s2s_enabled);

	return strnlen(buf, PAGE_SIZE);
}

static ssize_t sweep2sleep_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	if (sysfs_streq(buf, "1"))
		s2s_enabled = 1;
	else if (sysfs_streq(buf, "0"))
		s2s_enabled = 0;
	else
		return -EINVAL;

	return strnlen(buf, PAGE_SIZE);
}

static DEVICE_ATTR(sweep2sleep, (S_IRUGO | S_IWUSR),
	sweep2sleep_show, sweep2sleep_store);

static int __init sweep2sleep_init(void)
{
	int rc;

	sweep2sleep_pwrdev = input_allocate_device();
	if (!sweep2sleep_pwrdev) {
		pr_err("Can't allocate suspend autotest power button\n");
		return 0;
	}

	input_set_capability(sweep2sleep_pwrdev, EV_KEY, KEY_POWER);
	sweep2sleep_pwrdev->name = "s2s_pwrkey";
	sweep2sleep_pwrdev->phys = "s2s_pwrkey/input0";

	rc = input_register_device(sweep2sleep_pwrdev);
	if (rc) {
		pr_err("input_register_device err=%d\n", rc);
		input_free_device(sweep2sleep_pwrdev);
		return 0;
	}

	s2s_input_wq = create_workqueue("s2siwq");
	if (!s2s_input_wq) {
		pr_err("Failed to create s2siwq workqueue\n");
		return -EFAULT;
	}

	INIT_WORK(&s2s_input_work, s2s_input_callback);

	rc = input_register_handler(&s2s_input_handler);
	if (rc)
		pr_err("Failed to register s2s_input_handler\n");

	sweep2sleep_kobj = kobject_create_and_add("sweep2sleep", NULL) ;
	if (!sweep2sleep_kobj) {
		pr_err("sweep2sleep_kobj create_and_add failed\n");
		return -ENOMEM;
	}

	rc = sysfs_create_file(sweep2sleep_kobj, &dev_attr_sweep2sleep.attr);
	if (rc) {
		pr_err("sysfs_create_file failed for sweep2sleep\n");
		kobject_del(sweep2sleep_kobj);
		kobject_put(sweep2sleep_kobj);
		return rc;
	}

	return 0;
}

static void __exit sweep2sleep_exit(void)
{
	cancel_work_sync(&s2s_input_work);
	flush_workqueue(s2s_input_wq);
	destroy_workqueue(s2s_input_wq);

	sysfs_remove_file(sweep2sleep_kobj, &dev_attr_sweep2sleep.attr);
	kobject_del(sweep2sleep_kobj);
	kobject_put(sweep2sleep_kobj);

	input_unregister_handler(&s2s_input_handler);
	input_unregister_device(sweep2sleep_pwrdev);
	input_free_device(sweep2sleep_pwrdev);
}

module_init(sweep2sleep_init);
module_exit(sweep2sleep_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPLv2");
