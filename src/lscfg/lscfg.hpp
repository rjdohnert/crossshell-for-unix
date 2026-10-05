#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <vector>

struct VpdAttribute {
    std::string key;
    std::string value;
};

struct HardwareDevice {
    std::string resourceName; // e.g., sys0, proc0, mem0, hdisk0
    std::string location;     // e.g., CPU Socket 0, DIMM 1, PCI Slot 2
    std::string description;  // e.g., Intel(R) Core(TM) i9-13900K
    std::string deviceClass;  // sys, cpu, mem, disk, net, gpu, board
    std::vector<VpdAttribute> vpdAttributes;
};

enum class OutputFormat { Table, Csv, Json };
