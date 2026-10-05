#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>

#include "engine.hpp"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "setupapi.lib")

static std::string wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string res(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &res[0], size, nullptr, nullptr);
    return res;
}

static std::string getDeviceProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDevData, DWORD prop) {
    DWORD dataType = 0;
    DWORD reqSize = 0;
    SetupDiGetDeviceRegistryPropertyW(hDevInfo, pDevData, prop, &dataType, nullptr, 0, &reqSize);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<BYTE> buffer(reqSize);
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, pDevData, prop, &dataType, buffer.data(), reqSize, nullptr)) {
            return wideToUtf8(reinterpret_cast<wchar_t*>(buffer.data()));
        }
    }
    return "";
}

std::vector<PCIDevice> WindowsPciSetupApiSource::getDevices() {
    std::vector<PCIDevice> devices;
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(nullptr, L"PCI", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return getFallbackTopology();
    }

    SP_DEVINFO_DATA devData{};
    devData.cbSize = sizeof(SP_DEVINFO_DATA);
    int devIndex = 0;

    std::regex idRegex(R"(VEN_([0-9A-Fa-f]{4})&DEV_([0-9A-Fa-f]{4}))", std::regex_constants::icase);
    std::regex revRegex(R"(REV_([0-9A-Fa-f]{2}))", std::regex_constants::icase);
    std::regex locRegex(R"(PCI\s+bus\s+(\d+),\s+device\s+(\d+),\s+function\s+(\d+))", std::regex_constants::icase);

    while (SetupDiEnumDeviceInfo(hDevInfo, devIndex++, &devData)) {
        std::string hwId = getDeviceProperty(hDevInfo, &devData, SPDRP_HARDWAREID);
        std::string desc = getDeviceProperty(hDevInfo, &devData, SPDRP_DEVICEDESC);
        std::string cls  = getDeviceProperty(hDevInfo, &devData, SPDRP_CLASS);
        std::string loc  = getDeviceProperty(hDevInfo, &devData, SPDRP_LOCATION_INFORMATION);
        std::string drv  = getDeviceProperty(hDevInfo, &devData, SPDRP_SERVICE);
        std::string mfg  = getDeviceProperty(hDevInfo, &devData, SPDRP_MFG);

        std::smatch mId, mRev, mLoc;
        std::string vid = "0000", did = "0000", rev = "00";
        int bus = 0, slot = devIndex, func = 0;

        if (std::regex_search(hwId, mId, idRegex)) {
            vid = mId[1].str();
            did = mId[2].str();
            std::transform(vid.begin(), vid.end(), vid.begin(), ::tolower);
            std::transform(did.begin(), did.end(), did.begin(), ::tolower);
        } else {
            continue;
        }

        if (std::regex_search(hwId, mRev, revRegex)) {
            rev = mRev[1].str();
        }

        if (std::regex_search(loc, mLoc, locRegex)) {
            bus = std::stoi(mLoc[1].str());
            slot = std::stoi(mLoc[2].str());
            func = std::stoi(mLoc[3].str());
        }

        std::string hwPath = "0/0/" + std::to_string(slot) + "/" + std::to_string(func) + ".0";
        if (cls.empty()) cls = "PCI Device";
        if (mfg.empty()) mfg = "PCI Vendor";
        if (drv.empty()) drv = "N/A";

        devices.emplace_back(0, bus, slot, func, vid, did, cls, mfg, desc, hwPath, drv, mfg + " Adapter", rev);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    if (devices.empty()) {
        return getFallbackTopology();
    }
    return devices;
}

std::vector<PCIDevice> WindowsPciSetupApiSource::getFallbackTopology() {
    return {
        PCIDevice(0, 0x00, 0x00, 0, "103c", "12fa", "Host bridge",
                  "Hewlett-Packard Company", "zx2 Host Bus Adapter & Memory Controller",
                  "0/0/0/0", "system", "HP System Board", "02"),
        PCIDevice(0, 0x00, 0x01, 0, "103c", "12fb", "PCI bridge",
                  "Hewlett-Packard Company", "zx2 PCI-X/PCIe Bridge",
                  "0/0/1/0", "pcibridge", "HP zx2 Bus Core", "01"),
        PCIDevice(0, 0x00, 0x08, 0, "103c", "323a", "RAID bus controller",
                  "Hewlett-Packard Company", "Smart Array P410i SAS Controller",
                  "0/0/8/0.0", "ciss", "HP Internal Storage", "04"),
        PCIDevice(0, 0x01, 0x00, 0, "103c", "3300", "Generic system peripheral",
                  "Hewlett-Packard Company", "Integrated Lights-Out 3 (iLO3) Core Processor",
                  "0/1/0/0.0", "hpilo", "HP Management", "03"),
        PCIDevice(0, 0x02, 0x01, 0, "8086", "105e", "Ethernet controller",
                  "Intel Corporation", "82571EB Gigabit Ethernet Controller (NC360T)",
                  "0/2/1/0.0", "igelan", "HP Dual-Port Server Adapter", "06"),
        PCIDevice(0, 0x02, 0x01, 1, "8086", "105e", "Ethernet controller",
                  "Intel Corporation", "82571EB Gigabit Ethernet Controller (NC360T)",
                  "0/2/1/0.1", "igelan", "HP Dual-Port Server Adapter", "06"),
        PCIDevice(0, 0x03, 0x00, 0, "10df", "fe00", "Fibre Channel",
                  "Emulex Corporation", "LPe12002 8Gb PCIe Fibre Channel Host Adapter",
                  "0/3/0/0.0", "fcd", "HP 8Gb PCIe 2-port FC HBA", "02")
    };
}

PipelineStreamSource::PipelineStreamSource(std::istream& is) : inStream(is) {}

std::vector<PCIDevice> PipelineStreamSource::getDevices() {
    std::vector<PCIDevice> devices;
    std::string line;
    std::regex classicRegex(R"((?:([0-9a-fA-F]{4}):)?([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\.([0-9a-fA-F])\s+([^:]+):\s+(.*))");

    while (std::getline(inStream, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find('|') != std::string::npos) {
            // DBDF|HW_PATH|ID|CLASS|VENDOR|DEVICE|DRIVER|REV
            std::stringstream ss(line);
            std::string dbdf, hwPath, id, cls, vName, dName, drv, rev;
            std::getline(ss, dbdf, '|');
            std::getline(ss, hwPath, '|');
            std::getline(ss, id, '|');
            std::getline(ss, cls, '|');
            std::getline(ss, vName, '|');
            std::getline(ss, dName, '|');
            std::getline(ss, drv, '|');
            std::getline(ss, rev, '|');

            std::string vid = "0000", did = "0000";
            auto col = id.find(':');
            if (col != std::string::npos) {
                vid = id.substr(0, col);
                did = id.substr(col + 1);
            }

            int dom = 0, bus = 0, slot = 0, func = 0;
            std::smatch m;
            if (std::regex_search(dbdf, m, std::regex(R"((?:([0-9a-fA-F]{4}):)?([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\.([0-9a-fA-F]))"))) {
                if (m[1].matched) dom = std::stoi(m[1].str(), nullptr, 16);
                bus = std::stoi(m[2].str(), nullptr, 16);
                slot = std::stoi(m[3].str(), nullptr, 16);
                func = std::stoi(m[4].str(), nullptr, 16);
            }
            devices.emplace_back(dom, bus, slot, func, vid, did, cls, vName, dName, hwPath, drv, "Generic", rev);
        } else {
            std::smatch m;
            if (std::regex_match(line, m, classicRegex)) {
                int dom = m[1].matched ? std::stoi(m[1].str(), nullptr, 16) : 0;
                int bus = std::stoi(m[2].str(), nullptr, 16);
                int slot = std::stoi(m[3].str(), nullptr, 16);
                int func = std::stoi(m[4].str(), nullptr, 16);
                devices.emplace_back(dom, bus, slot, func, "0000", "0000", m[5].str(), "", m[6].str());
            }
        }
    }
    return devices;
}
