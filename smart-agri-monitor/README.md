# Smart Agriculture Monitoring System

A Linux-only capstone project in **C (kernel driver)** and **C++ (user-space app)**.
A character device driver emulates field sensors (soil moisture, temperature,
humidity) and an irrigation pump; a C++ application reads them through
`ioctl()`, shows live data, raises alerts and logs to CSV.

## Features
- Linux character device driver `/dev/agri_sensor` (`read`, `ioctl`)
- Automatic irrigation with threshold + hysteresis; manual pump override
- `/proc/agri_status` live status
- C++17 monitor app: live table, alerts, CSV logging, simulation mode
- Only C/C++ used, Linux only

## Project structure
```
smart-agri-monitor/
├── README.md
├── Makefile              # builds everything
├── .gitignore
├── include/agri_ioctl.h  # shared ioctl definitions
├── driver/               # kernel module (C)
│   ├── agri_driver.c
│   └── Makefile
├── app/                  # user-space app (C++)
│   ├── agri_monitor.cpp
│   └── Makefile
├── scripts/run_all.sh    # build + load + run + unload
├── data/                 # CSV logs are written here
└── docs/ARCHITECTURE.md
```

## Requirements
Linux (Ubuntu recommended, VM is fine), `gcc`, `g++`, `make`, kernel headers:
```
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

## Build
```
make            # builds driver/agri_driver.ko and app/agri_monitor
```

## Run
```
sudo insmod driver/agri_driver.ko
ls -l /dev/agri_sensor
cat /dev/agri_sensor              # one text reading
cat /proc/agri_status
./app/agri_monitor -n 15 -i 1     # 15 samples, 1 s apart
./app/agri_monitor -p on          # manual pump ON
./app/agri_monitor -p off
dmesg | tail                      # kernel log
sudo rmmod agri_driver
```
If `/dev/agri_sensor` needs root: `sudo ./app/agri_monitor ...` or `sudo chmod 666 /dev/agri_sensor`.

**Quick test without the driver:** `./app/agri_monitor -s -n 10`
**Everything in one go:** `./scripts/run_all.sh`

## Options
| Flag | Meaning |
|---|---|
| `-n N` | number of samples (default 10) |
| `-i S` | interval in seconds (default 1) |
| `-l FILE` | CSV log file (default `data/sensor_log.csv`) |
| `-p on/off` | set pump manually and exit |
| `-s` | simulation mode (no driver) |

## Sample output
```
[mode] KERNEL DRIVER /dev/agri_sensor

TIME                 MOISTURE%  TEMP(C)  HUMIDITY%  PUMP   ALERT
---------------------------------------------------------------------
2026-10-04 15:20:56  50         27       61         OFF
2026-10-04 15:20:58  48         28       62         OFF
2026-10-04 15:21:30  34         29       60         ON     DRY SOIL - irrigating
```
(Add your own screenshot in `docs/` after running.)

## How it works
See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Author
Subhaprada — B.Tech CSE, ITER, SOA University
