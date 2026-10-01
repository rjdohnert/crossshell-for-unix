/*
 * Copyright (c) 2026. Roberto J Dohnert. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <comdef.h>
#include <Wbemidl.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cctype>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// --- String Conversion Helpers ---
std::string WideToNarrow(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, NULL, NULL);
    return str;
}

std::string VariantToString(const VARIANT& vt) {
    if (vt.vt == VT_NULL || vt.vt == VT_EMPTY) return "N/A";
    if (vt.vt == VT_BSTR) return WideToNarrow(vt.bstrVal);
    if (vt.vt == VT_I4) return std::to_string(vt.lVal);
    if (vt.vt == VT_UI4) return std::to_string(vt.ulVal);
    if (vt.vt == VT_UI8) return std::to_string(vt.ullVal);
    if (vt.vt == VT_BOOL) return (vt.boolVal == VARIANT_TRUE) ? "True" : "False";
    return "N/A";
}

std::string FormatBytes(unsigned long long bytes) {
    if (bytes == 0) return "N/A";
    const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
    int i = 0;
    double dBytes = static_cast<double>(bytes);
    while (dBytes >= 1024 && i < 4) {
        dBytes /= 1024;
        i++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << dBytes << " " << suffixes[i];
    return ss.str();
}

// --- Data Structures ---
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

// --- WMI Engine Class ---
class WmiEngine {
private:
    IWbemServices* pSvc = nullptr;
    IWbemLocator* pLoc = nullptr;
    bool initialized = false;

public:
    WmiEngine() {
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;

        hr = CoInitializeSecurity(
            NULL, -1, NULL, NULL,
            RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL, EOAC_NONE, NULL
        );

        hr = CoCreateInstance(
            CLSID_WbemLocator, 0,
            CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pLoc
        );

        if (FAILED(hr)) return;

        hr = pLoc->ConnectServer(
            _bstr_t(L"ROOT\\CIMV2"),
            NULL, NULL, 0, NULL, 0, 0, &pSvc
        );

        if (FAILED(hr)) return;

        hr = CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
            NULL, EOAC_NONE
        );

        if (SUCCEEDED(hr)) initialized = true;
    }

    ~WmiEngine() {
        if (pSvc) pSvc->Release();
        if (pLoc) pLoc->Release();
        CoUninitialize();
    }

    bool IsInitialized() const { return initialized; }

    std::vector<std::map<std::string, std::string>> Query(const std::wstring& wmiClass, const std::vector<std::wstring>& properties) {
        std::vector<std::map<std::string, std::string>> results;
        if (!initialized) return results;

        std::wstring query = L"SELECT ";
        for (size_t i = 0; i < properties.size(); ++i) {
            query += properties[i];
            if (i < properties.size() - 1) query += L", ";
        }
        query += L" FROM " + wmiClass;

        IEnumWbemClassObject* pEnumerator = nullptr;
        HRESULT hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t(query.c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL, &pEnumerator
        );

        if (FAILED(hr) || !pEnumerator) return results;

        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;

        while (pEnumerator) {
            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn) break;

            std::map<std::string, std::string> row;
            for (const auto& prop : properties) {
                VARIANT vtProp;
                VariantInit(&vtProp);
                hr = pclsObj->Get(prop.c_str(), 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr)) {
                    row[WideToNarrow(prop)] = VariantToString(vtProp);
                }
                VariantClear(&vtProp);
            }
            results.push_back(row);
            pclsObj->Release();
        }
        pEnumerator->Release();
        return results;
    }
};

// --- Hardware Inventory Collector ---
class InventoryCollector {
public:
    static std::vector<HardwareDevice> Collect(WmiEngine& wmi) {
        std::vector<HardwareDevice> devices;

        // 1. System / BIOS / BaseBoard
        auto sysList = wmi.Query(L"Win32_ComputerSystem", { L"Manufacturer", L"Model", L"SystemType" });
        auto biosList = wmi.Query(L"Win32_BIOS", { L"Manufacturer", L"Name", L"SMBIOSBIOSVersion", L"SerialNumber", L"ReleaseDate" });
        auto boardList = wmi.Query(L"Win32_BaseBoard", { L"Manufacturer", L"Product", L"SerialNumber" });

        HardwareDevice sysDev;
        sysDev.resourceName = "sys0";
        sysDev.location = "System Planar";
        sysDev.deviceClass = "sys";
        sysDev.description = (!sysList.empty()) ? (sysList[0]["Manufacturer"] + " " + sysList[0]["Model"]) : "Windows Workstation System";
        if (!sysList.empty()) {
            sysDev.vpdAttributes.push_back({ "System Model", sysList[0]["Model"] });
            sysDev.vpdAttributes.push_back({ "System Architecture", sysList[0]["SystemType"] });
        }
        if (!biosList.empty()) {
            sysDev.vpdAttributes.push_back({ "BIOS Vendor", biosList[0]["Manufacturer"] });
            sysDev.vpdAttributes.push_back({ "BIOS Version", biosList[0]["SMBIOSBIOSVersion"] });
            sysDev.vpdAttributes.push_back({ "System Serial Number", biosList[0]["SerialNumber"] });
        }
        if (!boardList.empty()) {
            sysDev.vpdAttributes.push_back({ "Motherboard Product", boardList[0]["Product"] });
            sysDev.vpdAttributes.push_back({ "Motherboard Serial", boardList[0]["SerialNumber"] });
        }
        devices.push_back(sysDev);

        // 2. Processors (CPU)
        auto cpuList = wmi.Query(L"Win32_Processor", { L"DeviceID", L"Name", L"Manufacturer", L"NumberOfCores", L"NumberOfLogicalProcessors", L"MaxClockSpeed", L"SocketDesignation" });
        int cpuIdx = 0;
        for (const auto& c : cpuList) {
            HardwareDevice dev;
            dev.resourceName = "proc" + std::to_string(cpuIdx++);
            dev.location = (c.at("SocketDesignation") != "N/A") ? c.at("SocketDesignation") : "CPU Socket";
            dev.description = c.at("Name");
            dev.deviceClass = "cpu";
            dev.vpdAttributes.push_back({ "Manufacturer", c.at("Manufacturer") });
            dev.vpdAttributes.push_back({ "Physical Cores", c.at("NumberOfCores") });
            dev.vpdAttributes.push_back({ "Logical Threads", c.at("NumberOfLogicalProcessors") });
            dev.vpdAttributes.push_back({ "Max Clock Speed", c.at("MaxClockSpeed") + " MHz" });
            devices.push_back(dev);
        }

        // 3. Physical Memory (RAM)
        auto memList = wmi.Query(L"Win32_PhysicalMemory", { L"BankLabel", L"DeviceLocator", L"Capacity", L"Speed", L"Manufacturer", L"PartNumber", L"SerialNumber" });
        int memIdx = 0;
        for (const auto& m : memList) {
            HardwareDevice dev;
            dev.resourceName = "mem" + std::to_string(memIdx++);
            dev.location = (m.at("DeviceLocator") != "N/A") ? m.at("DeviceLocator") : ("Slot " + m.at("BankLabel"));
            unsigned long long cap = 0;
            try { cap = std::stoull(m.at("Capacity")); } catch (...) {}
            dev.description = FormatBytes(cap) + " Physical Memory Module";
            dev.deviceClass = "mem";
            dev.vpdAttributes.push_back({ "Manufacturer", m.at("Manufacturer") });
            dev.vpdAttributes.push_back({ "Part Number", m.at("PartNumber") });
            dev.vpdAttributes.push_back({ "Serial Number", m.at("SerialNumber") });
            dev.vpdAttributes.push_back({ "Configured Speed", m.at("Speed") + " MT/s" });
            devices.push_back(dev);
        }

        // 4. Storage Disks
        auto diskList = wmi.Query(L"Win32_DiskDrive", { L"DeviceID", L"Model", L"InterfaceType", L"Size", L"SerialNumber", L"MediaType" });
        int diskIdx = 0;
        for (const auto& d : diskList) {
            HardwareDevice dev;
            dev.resourceName = "hdisk" + std::to_string(diskIdx++);
            dev.location = d.at("InterfaceType") + " Controller";
            unsigned long long size = 0;
            try { size = std::stoull(d.at("Size")); } catch (...) {}
            dev.description = d.at("Model") + " (" + FormatBytes(size) + ")";
            dev.deviceClass = "disk";
            dev.vpdAttributes.push_back({ "Model / Machine Type", d.at("Model") });
            dev.vpdAttributes.push_back({ "Serial Number", d.at("SerialNumber") });
            dev.vpdAttributes.push_back({ "Bus Interface", d.at("InterfaceType") });
            dev.vpdAttributes.push_back({ "Media Type", d.at("MediaType") });
            devices.push_back(dev);
        }

        // 5. Network Adapters
        auto netList = wmi.Query(L"Win32_NetworkAdapter", { L"Name", L"AdapterType", L"MACAddress", L"Manufacturer", L"PNPDeviceID" });
        int netIdx = 0;
        for (const auto& n : netList) {
            if (n.at("MACAddress") == "N/A" || n.at("Name").find("Virtual") != std::string::npos) continue;
            HardwareDevice dev;
            dev.resourceName = "ent" + std::to_string(netIdx++);
            dev.location = "PCI Bus Adapter";
            dev.description = n.at("Name");
            dev.deviceClass = "net";
            dev.vpdAttributes.push_back({ "Manufacturer", n.at("Manufacturer") });
            dev.vpdAttributes.push_back({ "Network Address (MAC)", n.at("MACAddress") });
            dev.vpdAttributes.push_back({ "PNP Device ID", n.at("PNPDeviceID") });
            devices.push_back(dev);
        }

        // 6. GPUs / Display Controllers
        auto gpuList = wmi.Query(L"Win32_VideoController", { L"Name", L"DriverVersion", L"AdapterRAM", L"VideoProcessor" });
        int gpuIdx = 0;
        for (const auto& g : gpuList) {
            HardwareDevice dev;
            dev.resourceName = "gpu" + std::to_string(gpuIdx++);
            dev.location = "PCIe Graphics Slot";
            dev.description = g.at("Name");
            dev.deviceClass = "gpu";
            dev.vpdAttributes.push_back({ "Video Processor", g.at("VideoProcessor") });
            dev.vpdAttributes.push_back({ "Driver Version", g.at("DriverVersion") });
            unsigned long long vram = 0;
            try { vram = std::stoull(g.at("AdapterRAM")); } catch (...) {}
            dev.vpdAttributes.push_back({ "VRAM Capacity", FormatBytes(vram) });
            devices.push_back(dev);
        }

        return devices;
    }
};

// --- Comprehensive Help Section ---
void PrintVersion() {
    std::cout << "lscfg v1.0.0\n";
    std::cout << "Copyright (C) 2026, Roberto J Dohnert\n";
    std::cout << "Licensed under the BSD 3-Clause License\n";
}

void PrintHelp(const char* exeName) {
    std::cout << "\n";
    std::cout << "                           lscfg v1.0.0                   \n";
    std::cout << "\n\n";
    std::cout << "Queries hardware devices, physical slot locations, and Vital Product Data\n";
    std::cout << "(VPD) such as Serial Numbers, Part Numbers, Firmware/Driver levels, and MACs.\n\n";

    std::cout << "USAGE:\n";
    std::cout << "  " << exeName << " [OPTIONS]\n\n";

    std::cout << "OPTIONS:\n";
    std::cout << "  -v, --verbose           Display Vital Product Data (VPD) for installed resources.\n";
    std::cout << "  -s, --summary           Display compact 1-line resource summary output.\n";
    std::cout << "      --table             Display aligned table output (default).\n";
    std::cout << "      --csv               Display CSV output.\n";
    std::cout << "      --json              Display JSON output.\n";
    std::cout << "      -                   Read filters from standard input.\n";
    std::cout << "  -l, --line <class|name> Filter listing by specific device class or resource name.\n";
    std::cout << "                          Supported classes: sys, cpu, mem, disk, net, gpu.\n";
    std::cout << "      --version           Display version information and exit.\n";
    std::cout << "  -h, --help              Display this comprehensive help documentation and exit.\n\n";

    std::cout << "SUPPORTED DEVICE CLASSES (-l):\n";
    std::cout << "  sys    : System Motherboard, BIOS, SMBIOS, and Planar VPD\n";
    std::cout << "  cpu    : Processor Sockets, Cores, Threads, and Clock Speeds\n";
    std::cout << "  mem    : Physical RAM DIMMs, Speeds, Part Numbers, and Serials\n";
    std::cout << "  disk   : Storage Controllers, Hard Drives, and NVMe SSDs\n";
    std::cout << "  net    : Network Interface Cards (NICs) and MAC Addresses\n";
    std::cout << "  gpu    : Video Display Controllers, VRAM, and Driver Versions\n\n";

    std::cout << "AIX OUTPUT LAYOUT FORMAT:\n";
    std::cout << "  INSTALLED RESOURCE LIST\n";
    std::cout << "    RESOURCE         LOCATION            DESCRIPTION\n";
    std::cout << "    sys0             System Planar       Dell Inc. XPS 15 9520\n";
    std::cout << "      Manufacturer...................Dell Inc.\n";
    std::cout << "      System Serial Number...........1234567\n\n";

    std::cout << "EXAMPLES:\n";
    std::cout << "  " << exeName << "                      List all hardware resources (Summary Mode)\n";
    std::cout << "  " << exeName << " -v                   List all hardware resources with detailed VPD\n";
    std::cout << "  " << exeName << " -v -l mem            Display detailed VPD for RAM memory modules only\n";
    std::cout << "  " << exeName << " -v -l hdisk0         Display detailed VPD for primary storage disk\n";
    std::cout << "  " << exeName << " -s -l cpu            Display quick summary of CPU resources\n\n";
}

// --- Main Program Entry ---
int main(int argc, char* argv[]) {
    bool verbose = false;
    bool summaryOnly = false;
    std::string filter = "";
    enum class OutputFormat { Table, Csv, Json } format = OutputFormat::Table;

    // Parse Command-Line Options
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp(argv[0]);
            return 0;
        } else if (arg == "--version") {
            PrintVersion();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-s" || arg == "--summary") {
            summaryOnly = true;
        } else if (arg == "--table") {
            format = OutputFormat::Table;
        } else if (arg == "--csv") {
            format = OutputFormat::Csv;
        } else if (arg == "--json") {
            format = OutputFormat::Json;
        } else if (arg == "-") {
            std::getline(std::cin, filter, '\0');
            std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        } else if (!arg.empty() && arg[0] != '-') {
            filter = arg;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        } else if ((arg == "-l" || arg == "--line") && i + 1 < argc) {
            filter = argv[++i];
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        }
    }

    WmiEngine wmi;
    if (!wmi.IsInitialized()) {
        std::cerr << "Error: Failed to initialize Windows WMI COM interface.\n";
        return 1;
    }

    auto devices = InventoryCollector::Collect(wmi);

    // Apply Filter if Specified
    if (!filter.empty()) {
        std::vector<HardwareDevice> filtered;
        for (const auto& dev : devices) {
            std::string resLower = dev.resourceName;
            std::transform(resLower.begin(), resLower.end(), resLower.begin(), ::tolower);
            if (dev.deviceClass == filter || resLower == filter) {
                filtered.push_back(dev);
            }
        }
        devices = filtered;
    }

    if (format == OutputFormat::Csv) {
        std::cout << "Resource,Location,Description,Class\n";
        for (const auto& dev : devices) {
            auto quote = [](const std::string& value) { std::string out="\""; for(char c:value) out += c=='"' ? "\"\"" : std::string(1,c); return out+'"'; };
            std::cout << quote(dev.resourceName) << ',' << quote(dev.location) << ',' << quote(dev.description) << ',' << quote(dev.deviceClass) << '\n';
        }
        return 0;
    }
    if (format == OutputFormat::Json) {
        auto escape = [](const std::string& value) { std::string out; for(char c:value){if(c=='"'||c=='\\')out+='\\';if(c=='\n')out+="\\n";else out+=c;} return out; };
        std::cout << "[\n";
        for (size_t i=0; i<devices.size(); ++i) { const auto& dev=devices[i]; std::cout << "  {\"resource\":\"" << escape(dev.resourceName) << "\",\"location\":\"" << escape(dev.location) << "\",\"description\":\"" << escape(dev.description) << "\",\"class\":\"" << escape(dev.deviceClass) << "\"}" << (i+1==devices.size()?"\n":",\n"); }
        std::cout << "]\n";
        return 0;
    }

    // Print Header
    std::cout << "\nINSTALLED RESOURCE LIST\n\n";
    std::cout << "The following resources are installed on the system.\n";
    std::cout << "+/- = Added/Removed from last database update.\n\n";
    std::cout << "  RESOURCE        LOCATION                  DESCRIPTION\n";
    std::cout << "  -----------------------------------------------------------------------------\n";

    if (devices.empty()) {
        std::cout << "  No matching hardware resources found for filter: '" << filter << "'\n\n";
        return 0;
    }

    // Output Device Table
    for (const auto& dev : devices) {
        std::cout << "* " << std::left << std::setw(14) << dev.resourceName
                  << " " << std::setw(25) << dev.location
                  << " " << dev.description << "\n";

        // Print Vital Product Data (VPD) if Verbose (-v) is set and summaryOnly is false
        if (verbose && !summaryOnly) {
            for (const auto& vpd : dev.vpdAttributes) {
                std::string keyLabel = vpd.key;
                int dots = 30 - static_cast<int>(keyLabel.length());
                if (dots < 2) dots = 2;
                std::string dotPadding(dots, '.');

                std::cout << "        " << keyLabel << dotPadding << " " << vpd.value << "\n";
            }
            std::cout << "\n";
        }
    }
    std::cout << "\n";

    return 0;
}