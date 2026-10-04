# Smart Agriculture Monitoring System

A Linux-only capstone project written in **C (kernel device driver)** and **C++ (user-space application)**.

A character device driver emulates field sensors (soil moisture, temperature, humidity) and an irrigation pump. A C++ application reads them through `ioctl()`, shows a live table, raises alerts and logs the data to CSV.

## Features
- Linux character device driver exposing `/dev/agri_sensor` (`read`, `ioctl`)
- Live status in `/proc/agri_status`
- Automatic irrigation using a moisture threshold with hysteresis (ON below 35%, OFF above 60%)
- Manual pump override from user space
- C++17 monitor app: live table, alerts, CSV logging, simulation mode
- Uses only C/C++ and runs only on Linux

## Project Structure
```
Smart-Agriculture-Monitoring-System/
├── README.md
└── smart-agri-monitor/
    ├── Makefile                # builds driver + app
    ├── include/agri_ioctl.h    # shared ioctl definitions
    ├── driver/                 # Linux kernel module (C)
    │   ├── agri_driver.c
    │   └── Makefile
    ├── app/                    # user-space application (C++)
    │   ├── agri_monitor.cpp
    │   └── Makefile
    ├── scripts/run_all.sh      # build + load + run + unload
    ├── data/                   # CSV logs are written here
    └── docs/ARCHITECTURE.md    # architecture and driver concepts
```

## Architecture
```
 agri_monitor (C++)  --open/read/ioctl-->  /dev/agri_sensor
                                                 |
                                   agri_driver.ko (kernel module)
                                    - cdev + class + device node
                                    - mutex-protected sensor state
                                    - workqueue samples sensors every 1 s
                                    - auto/manual pump control
                                    - /proc/agri_status
```
More detail: [smart-agri-monitor/docs/ARCHITECTURE.md](smart-agri-monitor/docs/ARCHITECTURE.md)

## Linux Driver Concepts Used
`alloc_chrdev_region`, `cdev_add`, `class_create` / `device_create`, `file_operations`,
`unlocked_ioctl` (`_IOR` / `_IOW`), `copy_to_user`, `get_user`, `mutex`, delayed workqueue,
module parameter, procfs entry.

## Requirements
Linux (Ubuntu recommended, a VM is fine):
```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

## Build
```bash
git clone https://github.com/SubhapradaKar/Smart-Agriculture-Monitoring-System.git
cd Smart-Agriculture-Monitoring-System/smart-agri-monitor
make
```
This produces `driver/agri_driver.ko` and `app/agri_monitor`.

## Run
```bash
sudo insmod driver/agri_driver.ko       # load the driver
ls -l /dev/agri_sensor                  # device node created
cat /dev/agri_sensor                    # one text reading
cat /proc/agri_status                   # live status
sudo ./app/agri_monitor -n 15 -i 1      # 15 samples, 1 s apart
sudo ./app/agri_monitor -p on           # manually turn pump ON
sudo ./app/agri_monitor -p off          # manually turn pump OFF
dmesg | tail                            # kernel log messages
sudo rmmod agri_driver                  # unload the driver
```

**Quick test without the driver (simulation mode):**
```bash
./app/agri_monitor -s -n 10
```

**Everything in one go:**
```bash
./scripts/run_all.sh
```

## Command-line Options
| Flag | Meaning |
|------|---------|
| `-n N` | number of samples (default 10) |
| `-i S` | interval in seconds (default 1) |
| `-l FILE` | CSV log file (default `data/sensor_log.csv`) |
| `-p on/off` | set the pump manually and exit |
| `-s` | simulation mode (no driver needed) |

## Sample Output
```
[mode] KERNEL DRIVER /dev/agri_sensor

TIME                 MOISTURE%  TEMP(C)  HUMIDITY%  PUMP   ALERT
---------------------------------------------------------------------
2026-10-04 15:20:56  50         27       61         OFF
2026-10-04 15:20:58  48         28       62         OFF
2026-10-04 15:21:30  34         29       60         ON     DRY SOIL - irrigating
```

## Output Screenshots
<!-- Add your own screenshots to a docs/ folder and link them here, e.g. -->
<!-- ![Output](smart-agri-monitor/docs/output.png) -->

## Author
Subhaprada Kar — B.Tech CSE, ITER, SOA University, Bhubaneswar
