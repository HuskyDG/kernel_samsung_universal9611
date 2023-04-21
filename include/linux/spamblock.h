#ifndef SPAMBLOCK_H
#define SPAMBLOCK_H

#define STUB(type, name) static inline void vararg_discard_##name(type t, ...) {}
#define CALL_STUB(name) vararg_discard_##name

#include <linux/device.h>
STUB(struct device *, device)
#undef dev_info
#undef dev_dbg
#define dev_info(dev, ...) CALL_STUB(device)(dev, ##__VA_ARGS__)
#define dev_dbg(dev, ...) CALL_STUB(device)(dev, ##__VA_ARGS__)

#include <linux/printk.h>
STUB(const char *, format)
#undef pr_info
#define pr_info(fmt, ...) CALL_STUB(format)(fmt, ##__VA_ARGS__)

#include <linux/pm_qos.h>
#define pm_qos_update_request(target, freq) (void)0
#define pm_qos_request(qos_class) 0
#endif
