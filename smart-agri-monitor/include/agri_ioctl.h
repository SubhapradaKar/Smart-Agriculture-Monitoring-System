/* Shared between the kernel driver and the user-space application. */
#ifndef AGRI_IOCTL_H
#define AGRI_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define AGRI_DEVICE_NAME "agri_sensor"
#define AGRI_DEVICE_PATH "/dev/agri_sensor"

struct agri_data {
	__s32 soil_moisture; /* percent, 0..100          */
	__s32 temperature;   /* degrees Celsius          */
	__s32 humidity;      /* percent, 0..100          */
	__u32 pump_on;       /* 1 = irrigation pump ON   */
};

#define AGRI_IOC_MAGIC    'a'
#define AGRI_IOC_READ_DATA _IOR(AGRI_IOC_MAGIC, 1, struct agri_data)
#define AGRI_IOC_SET_PUMP  _IOW(AGRI_IOC_MAGIC, 2, int)

#endif /* AGRI_IOCTL_H */
