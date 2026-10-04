// SPDX-License-Identifier: GPL-2.0
/*
 * agri_driver.c - Smart Agriculture Monitoring: Linux character device driver
 *
 * Emulates a field node with three sensors (soil moisture, temperature,
 * humidity) and one actuator (irrigation pump). Sensor values are refreshed
 * every second by a delayed work item (a stand-in for a real ADC/I2C read).
 * The pump is switched automatically by a moisture threshold (hysteresis),
 * or manually through ioctl.
 *
 * Interfaces exposed to user space:
 *   read()   -> one human-readable text line
 *   ioctl()  -> AGRI_IOC_READ_DATA (struct agri_data), AGRI_IOC_SET_PUMP
 *   /proc/agri_status
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#include "agri_ioctl.h"

#define MOISTURE_ON_BELOW  35   /* pump ON  when soil moisture < 35% */
#define MOISTURE_OFF_ABOVE 60   /* pump OFF when soil moisture > 60% */

static int moisture_threshold_low = MOISTURE_ON_BELOW;
module_param(moisture_threshold_low, int, 0644);
MODULE_PARM_DESC(moisture_threshold_low, "Pump turns ON below this soil moisture (%)");

static dev_t agri_devt;
static struct cdev agri_cdev;
static struct class *agri_class;
static struct device *agri_device;
static struct proc_dir_entry *agri_proc;

static DEFINE_MUTEX(agri_lock);
static struct agri_data state = {
	.soil_moisture = 50, .temperature = 28, .humidity = 60, .pump_on = 0,
};
static bool manual_override;
static struct delayed_work sensor_work;

static u32 rnd(u32 n)
{
	u32 v;

	get_random_bytes(&v, sizeof(v));
	return v % n;
}

static int clamp_pct(int v)
{
	return v < 0 ? 0 : (v > 100 ? 100 : v);
}

/* Periodic "sensor sampling" - runs in process context via workqueue. */
static void sensor_work_fn(struct work_struct *w)
{
	mutex_lock(&agri_lock);

	/* Soil dries slowly, pump adds water quickly. */
	if (state.pump_on)
		state.soil_moisture += 4 + rnd(3);
	else
		state.soil_moisture -= (int)rnd(3);
	state.soil_moisture = clamp_pct(state.soil_moisture);

	state.temperature += (int)rnd(3) - 1;           /* -1..+1 */
	if (state.temperature < 15) state.temperature = 15;
	if (state.temperature > 45) state.temperature = 45;

	state.humidity = clamp_pct(state.humidity + (int)rnd(5) - 2);

	/* Automatic irrigation control with hysteresis. */
	if (!manual_override) {
		if (state.soil_moisture < moisture_threshold_low)
			state.pump_on = 1;
		else if (state.soil_moisture > MOISTURE_OFF_ABOVE)
			state.pump_on = 0;
	}
	mutex_unlock(&agri_lock);

	schedule_delayed_work(&sensor_work, HZ);
}

static int agri_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int agri_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t agri_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	char line[128];
	int n;

	if (*off > 0)
		return 0;               /* EOF on second read */

	mutex_lock(&agri_lock);
	n = scnprintf(line, sizeof(line),
		      "moisture=%d%% temp=%dC humidity=%d%% pump=%s\n",
		      state.soil_moisture, state.temperature, state.humidity,
		      state.pump_on ? "ON" : "OFF");
	mutex_unlock(&agri_lock);

	if (n > len)
		n = len;
	if (copy_to_user(buf, line, n))
		return -EFAULT;
	*off += n;
	return n;
}

static long agri_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct agri_data snap;
	int pump;

	if (_IOC_TYPE(cmd) != AGRI_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case AGRI_IOC_READ_DATA:
		mutex_lock(&agri_lock);
		snap = state;
		mutex_unlock(&agri_lock);
		if (copy_to_user((void __user *)arg, &snap, sizeof(snap)))
			return -EFAULT;
		return 0;

	case AGRI_IOC_SET_PUMP:
		if (get_user(pump, (int __user *)arg))
			return -EFAULT;
		mutex_lock(&agri_lock);
		state.pump_on = pump ? 1 : 0;
		manual_override = true;   /* user took control */
		mutex_unlock(&agri_lock);
		pr_info("agri: pump manually set %s\n", pump ? "ON" : "OFF");
		return 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations agri_fops = {
	.owner          = THIS_MODULE,
	.open           = agri_open,
	.release        = agri_release,
	.read           = agri_read,
	.unlocked_ioctl = agri_ioctl,
};

static int agri_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&agri_lock);
	seq_printf(m, "soil_moisture : %d %%\ntemperature   : %d C\n"
		      "humidity      : %d %%\npump          : %s\nmode          : %s\n",
		   state.soil_moisture, state.temperature, state.humidity,
		   state.pump_on ? "ON" : "OFF",
		   manual_override ? "manual" : "auto");
	mutex_unlock(&agri_lock);
	return 0;
}

static int agri_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, agri_proc_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops agri_proc_ops = {
	.proc_open    = agri_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations agri_proc_ops = {
	.owner   = THIS_MODULE,
	.open    = agri_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};
#endif

static int __init agri_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&agri_devt, 0, 1, AGRI_DEVICE_NAME);
	if (ret)
		return ret;

	cdev_init(&agri_cdev, &agri_fops);
	agri_cdev.owner = THIS_MODULE;
	ret = cdev_add(&agri_cdev, agri_devt, 1);
	if (ret)
		goto err_region;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	agri_class = class_create(AGRI_DEVICE_NAME);
#else
	agri_class = class_create(THIS_MODULE, AGRI_DEVICE_NAME);
#endif
	if (IS_ERR(agri_class)) {
		ret = PTR_ERR(agri_class);
		goto err_cdev;
	}

	agri_device = device_create(agri_class, NULL, agri_devt, NULL, AGRI_DEVICE_NAME);
	if (IS_ERR(agri_device)) {
		ret = PTR_ERR(agri_device);
		goto err_class;
	}

	agri_proc = proc_create("agri_status", 0444, NULL, &agri_proc_ops);

	INIT_DELAYED_WORK(&sensor_work, sensor_work_fn);
	schedule_delayed_work(&sensor_work, HZ);

	pr_info("agri: loaded, major=%d minor=%d\n", MAJOR(agri_devt), MINOR(agri_devt));
	return 0;

err_class:
	class_destroy(agri_class);
err_cdev:
	cdev_del(&agri_cdev);
err_region:
	unregister_chrdev_region(agri_devt, 1);
	return ret;
}

static void __exit agri_exit(void)
{
	cancel_delayed_work_sync(&sensor_work);
	if (agri_proc)
		proc_remove(agri_proc);
	device_destroy(agri_class, agri_devt);
	class_destroy(agri_class);
	cdev_del(&agri_cdev);
	unregister_chrdev_region(agri_devt, 1);
	pr_info("agri: unloaded\n");
}

module_init(agri_init);
module_exit(agri_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Subhaprada");
MODULE_DESCRIPTION("Smart Agriculture Monitoring - sensor/pump character driver");
MODULE_VERSION("1.0");
