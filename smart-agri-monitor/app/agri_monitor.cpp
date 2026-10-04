// agri_monitor.cpp - user-space application for the Smart Agriculture Monitoring System
//
// Reads sensor data from the kernel driver (/dev/agri_sensor) through ioctl(),
// prints a live table, logs to CSV, and can control the pump.
//
// Usage:
//   ./agri_monitor [-n count] [-i seconds] [-l logfile] [-p on|off] [-s]
//     -n  number of samples (default 10)
//     -i  interval between samples in seconds (default 1)
//     -l  CSV log file (default data/sensor_log.csv)
//     -p  manually turn the pump on/off, then exit
//     -s  simulation mode (no driver needed; for quick testing)
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <random>

#include "../include/agri_ioctl.h"

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

// ---- Sensor source abstraction -------------------------------------------
class SensorSource {
public:
    virtual ~SensorSource() = default;
    virtual bool read(agri_data &d) = 0;
    virtual bool setPump(bool on) = 0;
};

// Talks to the real kernel driver.
class DeviceSource : public SensorSource {
    int fd_;
public:
    explicit DeviceSource(const char *path) : fd_(::open(path, O_RDWR)) {}
    ~DeviceSource() override { if (fd_ >= 0) ::close(fd_); }
    bool ok() const { return fd_ >= 0; }
    bool read(agri_data &d) override { return ioctl(fd_, AGRI_IOC_READ_DATA, &d) == 0; }
    bool setPump(bool on) override {
        int v = on ? 1 : 0;
        return ioctl(fd_, AGRI_IOC_SET_PUMP, &v) == 0;
    }
};

// Pure user-space simulation (same behaviour as the driver) for testing.
class SimSource : public SensorSource {
    agri_data s_{50, 28, 60, 0};
    std::mt19937 rng_{std::random_device{}()};
    int r(int n) { return (int)(rng_() % n); }
    static int pct(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
public:
    bool read(agri_data &d) override {
        s_.soil_moisture = pct(s_.soil_moisture + (s_.pump_on ? 4 + r(3) : -r(3)));
        s_.temperature  += r(3) - 1;
        s_.humidity      = pct(s_.humidity + r(5) - 2);
        if (s_.soil_moisture < 35) s_.pump_on = 1;
        else if (s_.soil_moisture > 60) s_.pump_on = 0;
        d = s_;
        return true;
    }
    bool setPump(bool on) override { s_.pump_on = on; return true; }
};

static std::string nowStr() {
    char b[32];
    std::time_t t = std::time(nullptr);
    std::strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return b;
}

int main(int argc, char **argv) {
    int count = 10, interval = 1;
    std::string logfile = "data/sensor_log.csv", pumpArg;
    bool sim = false;

    int opt;
    while ((opt = getopt(argc, argv, "n:i:l:p:sh")) != -1) {
        switch (opt) {
        case 'n': count = std::atoi(optarg); break;
        case 'i': interval = std::atoi(optarg); break;
        case 'l': logfile = optarg; break;
        case 'p': pumpArg = optarg; break;
        case 's': sim = true; break;
        default:
            std::cerr << "Usage: " << argv[0]
                      << " [-n count] [-i sec] [-l log.csv] [-p on|off] [-s]\n";
            return opt == 'h' ? 0 : 1;
        }
    }

    std::unique_ptr<SensorSource> src;
    if (sim) {
        src.reset(new SimSource);
        std::cout << "[mode] SIMULATION (no kernel driver)\n";
    } else {
        auto dev = new DeviceSource(AGRI_DEVICE_PATH);
        if (!dev->ok()) {
            std::cerr << "Cannot open " << AGRI_DEVICE_PATH << ": " << std::strerror(errno)
                      << "\nLoad the driver first (sudo insmod driver/agri_driver.ko) "
                         "or run with -s.\n";
            delete dev;
            return 1;
        }
        src.reset(dev);
        std::cout << "[mode] KERNEL DRIVER " << AGRI_DEVICE_PATH << "\n";
    }

    if (!pumpArg.empty()) {
        bool on = (pumpArg == "on");
        if (!src->setPump(on)) { std::perror("set pump"); return 1; }
        std::cout << "Pump set to " << (on ? "ON" : "OFF") << "\n";
        return 0;
    }

    std::signal(SIGINT, on_sigint);

    std::ofstream log(logfile, std::ios::app);
    if (!log) std::cerr << "Warning: cannot open log file " << logfile << "\n";
    else if (log.tellp() == 0) log << "time,moisture,temperature,humidity,pump\n";

    std::printf("\n%-20s %-10s %-8s %-10s %-6s %s\n",
                "TIME", "MOISTURE%", "TEMP(C)", "HUMIDITY%", "PUMP", "ALERT");
    std::printf("---------------------------------------------------------------------\n");

    for (int i = 0; i < count && !g_stop; ++i) {
        agri_data d{};
        if (!src->read(d)) { std::perror("read sensors"); return 1; }

        const char *alert = "";
        if (d.soil_moisture < 35)      alert = "DRY SOIL - irrigating";
        else if (d.temperature > 38)   alert = "HIGH TEMPERATURE";

        std::string t = nowStr();
        std::printf("%-20s %-10d %-8d %-10d %-6s %s\n", t.c_str(), d.soil_moisture,
                    d.temperature, d.humidity, d.pump_on ? "ON" : "OFF", alert);
        if (log) log << t << ',' << d.soil_moisture << ',' << d.temperature << ','
                     << d.humidity << ',' << (d.pump_on ? "ON" : "OFF") << '\n';
        log.flush();
        if (i + 1 < count) sleep(interval);
    }
    std::cout << "\nDone. Log saved to " << logfile << "\n";
    return 0;
}
