/*
 *	SoftDog:	A Software Watchdog Device
 *
 *	(c) Copyright 1996 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *							All Rights Reserved.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Alan Cox nor CymruNet Ltd. admit liability nor provide
 *	warranty for any of this software. This material is provided
 *	"AS-IS" and at no charge.
 *
 *	(c) Copyright 1995    Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 *	Software only watchdog driver. Unlike its big brother the WDT501P
 *	driver this won't always recover a failed machine.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/sec_debug.h>

#define TIMER_MARGIN	60		/* Default is 60 seconds */
#ifdef CONFIG_SEC_DEBUG
extern int force_softdog;
#endif
static unsigned int soft_margin = TIMER_MARGIN;	/* in seconds */
module_param(soft_margin, uint, 0);
MODULE_PARM_DESC(soft_margin,
	"Watchdog soft_margin in seconds. (0 < soft_margin < 65536, default="
					__MODULE_STRING(TIMER_MARGIN) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static int soft_noboot;
module_param(soft_noboot, int, 0);
MODULE_PARM_DESC(soft_noboot,
	"Softdog action, set to 1 to ignore reboots, 0 to reboot (default=0)");

static int soft_panic;
module_param(soft_panic, int, 0);
MODULE_PARM_DESC(soft_panic,
	"Softdog action, set to 1 to panic, 0 to reboot (default=0)");

static struct hrtimer softdog_ticktock;
static struct hrtimer softdog_preticktock;

static struct watchdog_device softdog_dev;

static enum hrtimer_restart softdog_fire(struct hrtimer *timer)
{
	module_put(THIS_MODULE);
	if (soft_noboot) {
		pr_crit("Triggered - Reboot ignored\n");
	} else if (soft_panic) {
		pr_crit("Initiating panic\n");
#ifdef CONFIG_SEC_DEBUG_SOFTDOG
		secdbg_softdog_show_info();
#endif
#if IS_ENABLED(CONFIG_SEC_DEBUG_SOFTDOG_PWDT)
		panic("Software Watchdog Timer expired %ds", softdog_dev.timeout);
#else
		panic("Software Watchdog Timer expired");
#endif
	} else {
		pr_crit("Initiating system reboot\n");
		emergency_restart();
		pr_crit("Reboot didn't ?????\n");
	}

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart softdog_pretimeout(struct hrtimer *timer)
{
	watchdog_notify_pretimeout(&softdog_dev);

	return HRTIMER_NORESTART;
}

static int softdog_ping(struct watchdog_device *w)
{
#ifdef CONFIG_SEC_DEBUG
	if (force_softdog == 1)
		return 0;
#endif
	if (!hrtimer_active(&softdog_ticktock))
		__module_get(THIS_MODULE);
	hrtimer_start(&softdog_ticktock, ktime_set(w->timeout, 0),
		      HRTIMER_MODE_REL);

	pr_info_ratelimited("%s: %u\n", __func__, w->timeout);

	if (IS_ENABLED(CONFIG_SOFT_WATCHDOG_PRETIMEOUT)) {
		if (w->pretimeout)
			hrtimer_start(&softdog_preticktock,
				      ktime_set(w->timeout - w->pretimeout, 0),
				      HRTIMER_MODE_REL);
		else
			hrtimer_cancel(&softdog_preticktock);
	}

	return 0;
}

static int softdog_stop(struct watchdog_device *w)
{
	if (hrtimer_cancel(&softdog_ticktock))
		module_put(THIS_MODULE);

	pr_info_ratelimited("%s: %u\n", __func__, w->timeout);

	if (IS_ENABLED(CONFIG_SOFT_WATCHDOG_PRETIMEOUT))
		hrtimer_cancel(&softdog_preticktock);

	return 0;
}

static struct watchdog_info softdog_info = {
	.identity = "Software Watchdog",
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
};

static const struct watchdog_ops softdog_ops = {
	.owner = THIS_MODULE,
	.start = softdog_ping,
	.stop = softdog_stop,
};

static struct watchdog_device softdog_dev = {
	.info = &softdog_info,
	.ops = &softdog_ops,
	.min_timeout = 1,
	.max_timeout = 65535,
	.timeout = TIMER_MARGIN,
};

static int __init softdog_init(void)
{
	int ret;

	watchdog_init_timeout(&softdog_dev, soft_margin, NULL);
	watchdog_set_nowayout(&softdog_dev, nowayout);
	watchdog_stop_on_reboot(&softdog_dev);

	hrtimer_init(&softdog_ticktock, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	softdog_ticktock.function = softdog_fire;

	if (IS_ENABLED(CONFIG_SOFT_WATCHDOG_PRETIMEOUT)) {
		softdog_info.options |= WDIOF_PRETIMEOUT;
		hrtimer_init(&softdog_preticktock, CLOCK_MONOTONIC,
			     HRTIMER_MODE_REL);
		softdog_preticktock.function = softdog_pretimeout;
	}

	ret = watchdog_register_device(&softdog_dev);
	if (ret)
		return ret;

	pr_info("initialized. soft_noboot=%d soft_margin=%d sec soft_panic=%d (nowayout=%d)\n",
		soft_noboot, softdog_dev.timeout, soft_panic, nowayout);

	return 0;
}
module_init(softdog_init);

static void __exit softdog_exit(void)
{
	watchdog_unregister_device(&softdog_dev);
}
module_exit(softdog_exit);

MODULE_AUTHOR("Alan Cox");
MODULE_DESCRIPTION("Software Watchdog Device Driver");
MODULE_LICENSE("GPL");
