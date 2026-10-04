# Architecture

## Block diagram

```
 +------------------------- USER SPACE ---------------------------+
 |  agri_monitor (C++)                                            |
 |   - SensorSource (abstract)                                    |
 |       |- DeviceSource  -> open/ioctl on /dev/agri_sensor       |
 |       `- SimSource     -> pure user-space test mode (-s)       |
 |   - table output, alerts, CSV logging (data/sensor_log.csv)    |
 +----------------------------+-----------------------------------+
                              |  system calls: open / read / ioctl
 +----------------------------v-----------------------------------+
 |                        KERNEL SPACE                            |
 |  VFS -> /dev/agri_sensor -> file_operations (agri_fops)        |
 |        agri_driver.ko (char device driver)                     |
 |          - cdev + class + device_create (auto /dev node)       |
 |          - mutex-protected shared state                        |
 |          - delayed_work: samples "sensors" every 1 s           |
 |          - pump control: auto (threshold + hysteresis) / manual|
 |          - /proc/agri_status                                   |
 +----------------------------+-----------------------------------+
                              |  (in real hardware: GPIO / I2C / ADC)
 +----------------------------v-----------------------------------+
 |  Soil-moisture sensor | DHT11 temp+humidity | Relay -> Pump    |
 +----------------------------------------------------------------+
```

## Driver concepts demonstrated
| Concept | Where |
|---|---|
| Char device registration (`alloc_chrdev_region`, `cdev_add`) | `agri_init` |
| Automatic `/dev` node (`class_create`, `device_create`) | `agri_init` |
| `file_operations`: open, release, read, unlocked_ioctl | `agri_fops` |
| `ioctl` command encoding (`_IOR`, `_IOW`) | `include/agri_ioctl.h` |
| `copy_to_user` / `get_user` | `agri_read`, `agri_ioctl` |
| Mutual exclusion (`mutex`) | `agri_lock` |
| Deferred periodic work (workqueue) | `sensor_work_fn` |
| Module parameter | `moisture_threshold_low` |
| procfs entry | `/proc/agri_status` |

## Control logic
Pump turns ON when soil moisture < 35 % and OFF when > 60 % (hysteresis,
prevents rapid toggling). Setting the pump with `-p` switches to manual mode.

## Hardware mapping (optional extension)
Replace the random generator in `sensor_work_fn` with reads from an ADC
(soil moisture), a DHT11 GPIO driver (temp/humidity) and a GPIO line to a
relay (pump), e.g. on a Raspberry Pi.
