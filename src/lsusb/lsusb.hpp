#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <string>
#include <vector>

class USBDevice {
public:
    int bus;
    int device;
    std::string vid;
    std::string pid;
    std::string description;
    std::string deviceClass;
    std::string speed;
    std::string hwPath;     // hardware path (e.g., 0/0/1/0.1)
    std::string serial;

    USBDevice(int b, int d, std::string v, std::string p,
              std::string desc, std::string cls = "Generic",
              std::string spd = "High (480M)", std::string path = "0/0/0/0",
              std::string sn = "N/A");

    std::string getId() const;
    bool matches(int filterBus,
                 int filterDev,
                 const std::string& filterVid,
                 const std::string& filterPid) const;
};
