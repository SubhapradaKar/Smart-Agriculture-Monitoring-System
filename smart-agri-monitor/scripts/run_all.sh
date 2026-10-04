#!/bin/bash
# Build driver + app, load the driver, run the monitor, unload.
set -e
cd "$(dirname "$0")/.."
make -C driver
make -C app
sudo insmod driver/agri_driver.ko
sleep 1
ls -l /dev/agri_sensor
cat /dev/agri_sensor
cat /proc/agri_status
./app/agri_monitor -n 15 -i 1
sudo rmmod agri_driver
dmesg | tail -5
