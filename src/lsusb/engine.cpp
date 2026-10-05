#include "engine.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

USBDevice::USBDevice(int b, int d, std::string v, std::string p,
          std::string desc, std::string cls,
          std::string spd, std::string path,
          std::string sn)
    : bus(b), device(d), vid(std::move(v)), pid(std::move(p)),
      description(std::move(desc)), deviceClass(std::move(cls)),
      speed(std::move(spd)), hwPath(std::move(path)), serial(std::move(sn)) {}

std::string USBDevice::getId() const {
    return vid + ":" + pid;
}

bool USBDevice::matches(int filterBus,
                           int filterDev,
                           const std::string& filterVid,
                           const std::string& filterPid) const {
    if (filterBus >= 0 && bus != filterBus) return false;
    if (filterDev >= 0 && device != filterDev) return false;
    if (!filterVid.empty() && vid != filterVid) return false;
    if (!filterPid.empty() && pid != filterPid) return false;
    return true;
}

std::string WindowsSetupApiSource::wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string res(static_cast<size_t>(size), '\0');
    int written = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &res[0], size, nullptr, nullptr);
    if (written <= 0) return {};
    res.resize(static_cast<size_t>(written));
    return res;
}

std::string WindowsSetupApiSource::getDeviceProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDevData, DWORD prop) {
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

std::vector<USBDevice> WindowsSetupApiSource::getDevices() {
    std::vector<USBDevice> devices;
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return getFallbackDevices();
    }

    SP_DEVINFO_DATA devData{};
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    int devIndex = 0;
    int simulatedBus = 1;
    int simulatedDev = 1;

    std::regex idRegex(R"(VID_([0-9A-Fa-f]{4})&PID_([0-9A-Fa-f]{4}))", std::regex_constants::icase);

    while (SetupDiEnumDeviceInfo(hDevInfo, devIndex++, &devData)) {
        std::string hwId = getDeviceProperty(hDevInfo, &devData, SPDRP_HARDWAREID);
        std::string desc = getDeviceProperty(hDevInfo, &devData, SPDRP_FRIENDLYNAME);
        if (desc.empty()) {
            desc = getDeviceProperty(hDevInfo, &devData, SPDRP_DEVICEDESC);
        }
        std::string cls = getDeviceProperty(hDevInfo, &devData, SPDRP_CLASS);
        std::string location = getDeviceProperty(hDevInfo, &devData, SPDRP_LOCATION_INFORMATION);

        std::smatch match;
        std::string vid = "0000";
        std::string pid = "0000";
        if (std::regex_search(hwId, match, idRegex)) {
            vid = match[1].str();
            pid = match[2].str();
            std::transform(vid.begin(), vid.end(), vid.begin(), ::tolower);
            std::transform(pid.begin(), pid.end(), pid.begin(), ::tolower);
        } else {
            continue;
        }

        std::string hwPath = "0/0/1/0." + std::to_string(simulatedDev);
        if (cls.empty()) cls = "USBDevice";
        if (desc.empty()) desc = "USB Input/Output Peripheral";

        devices.emplace_back(simulatedBus, simulatedDev++, vid, pid, desc, cls, "High (480M)", hwPath, "N/A");
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    if (devices.empty()) {
        return getFallbackDevices();
    }
    return devices;
}

std::vector<USBDevice> WindowsSetupApiSource::getFallbackDevices() {
    return {
        USBDevice(1, 1, "1d6b", "0002", "Linux Foundation 2.0 Root Hub", "Hub", "480M", "0/0/1/0", "0000:00:1a.0"),
        USBDevice(1, 2, "03f0", "7029", "HP Virtual Keyboard/Mouse (iLO)", "HID", "1.5M", "0/0/1/0.1.0", "HPiLO-KVM01"),
        USBDevice(1, 3, "03f0", "1027", "HP Virtual DVD-ROM Drive (iLO)", "Mass Storage", "480M", "0/0/1/0.2.0", "HPiLO-VMD01"),
        USBDevice(2, 1, "1d6b", "0003", "Linux Foundation 3.0 Root Hub", "Hub", "5000M", "0/0/2/0", "0000:00:1d.0"),
        USBDevice(2, 2, "03f0", "0038", "HP Enterprise 64GB Flash Drive", "Mass Storage", "5000M", "0/0/2/0.1.0", "HP99841284"),
        USBDevice(2, 3, "0403", "6001", "FTDI FT232R USB UART Serial Bridge", "Communications", "12M", "0/0/2/0.2.0", "FT992A0B")
    };
}

PipelineStreamSource::PipelineStreamSource(std::istream& is) : inStream(is) {}

std::vector<USBDevice> PipelineStreamSource::getDevices() {
    std::vector<USBDevice> devices;
    std::string line;
    std::regex classicRegex(R"(Bus\s+(\d+)\s+Device\s+(\d+):\s+ID\s+([0-9a-fA-F]{4}):([0-9a-fA-F]{4})\s+(.*))");

    while (std::getline(inStream, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find('|') != std::string::npos) {
            std::stringstream ss(line);
            std::string b, d, id, cls, spd, path, desc, sn;
            std::getline(ss, b, '|');
            std::getline(ss, d, '|');
            std::getline(ss, id, '|');
            std::getline(ss, cls, '|');
            std::getline(ss, spd, '|');
            std::getline(ss, path, '|');
            std::getline(ss, desc, '|');
            std::getline(ss, sn, '|');

            std::string vid = "0000", pid = "0000";
            auto colonPos = id.find(':');
            if (colonPos != std::string::npos) {
                vid = id.substr(0, colonPos);
                pid = id.substr(colonPos + 1);
            }
            devices.emplace_back(std::stoi(b), std::stoi(d), vid, pid, desc, cls, spd, path, sn);
        } else {
            std::smatch match;
            if (std::regex_match(line, match, classicRegex)) {
                devices.emplace_back(std::stoi(match[1].str()), std::stoi(match[2].str()),
                                     match[3].str(), match[4].str(), match[5].str());
            }
        }
    }
    return devices;
}
