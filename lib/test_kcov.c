// SPDX-License-Identifier: GPL-2.0

#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kcov.h>
#include <linux/refcount.h>
#include <linux/log2.h>
#include <linux/timer.h>
#include <asm/setup.h>

static void kcov_test_timer_handler(struct timer_list *t);

DEFINE_TIMER(kcov_test_timer, kcov_test_timer_handler);

static void kcov_test_timer_handler(struct timer_list *t)
{
	kcov_remote_start_usb(jiffies % 8);
	pr_err("! kcov_test_init: bus = %lu\n", jiffies % 8);
        mod_timer(&kcov_test_timer, jiffies + (HZ >> 10));
	kcov_remote_stop();
}


#define KCOV_TEST_START_THREAD 0x42
#define KCOV_TEST_START_TIMER 0x43

struct kcov_test {
	spinlock_t lock;
};

static int kcov_test_open(struct inode *inode, struct file *filep)
{
	struct kcov_test *kcov_test;

	kcov_test = kzalloc(sizeof(*kcov_test), GFP_KERNEL);
	if (!kcov_test)
		return -ENOMEM;
	spin_lock_init(&kcov_test->lock);
	filep->private_data = kcov_test;
	return nonseekable_open(inode, filep);
}

static int kcov_test_close(struct inode *inode, struct file *filep)
{
	kfree(filep->private_data);
	return 0;
}

static int kcov_test_ioctl_locked(struct kcov_test *kcov_test, unsigned int cmd,
			     			unsigned long arg)
{
	switch (cmd) {
	case KCOV_TEST_START_THREAD:
		return 0;
	case KCOV_TEST_START_TIMER:
		return 0;
	default:
		return -ENOTTY;
	}
}

static long kcov_test_ioctl(struct file *filep, unsigned int cmd,
					unsigned long arg)
{
	struct kcov_test *kcov_test;
	int res;
	unsigned long flags;

	kcov_test = filep->private_data;
	spin_lock_irqsave(&kcov_test->lock, flags);
	res = kcov_test_ioctl_locked(kcov_test, cmd, arg);
	spin_unlock_irqrestore(&kcov_test->lock, flags);

	return res;
}

static const struct file_operations kcov_test_fops = {
	.open		= kcov_test_open,
	.unlocked_ioctl	= kcov_test_ioctl,
	.release        = kcov_test_close,
};

static int __init kcov_test_init(void)
{
	debugfs_create_file_unsafe("kcov_test", 0600, NULL, NULL,
					&kcov_test_fops);

	pr_err("! kcov_test_init: timer init\n");
        mod_timer(&kcov_test_timer, jiffies + HZ);

	return 0;
}

device_initcall(kcov_test_init);
