#include "engine.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <sstream>

namespace StringUtil {
    std::string ToLower(const std::string& str) {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    std::string WideToAnsi(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size, nullptr, nullptr);
        return str;
    }

    std::vector<std::string> Split(const std::string& str, char delim) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream stream(str);
        while (std::getline(stream, token, delim)) {
            tokens.push_back(token);
        }
        return tokens;
    }
}

Device::Device(std::string logicalName,
       std::string state,
       std::string location,
       std::string description,
       std::string devClass,
       std::string subClass,
       std::string devType)
    : m_logicalName(std::move(logicalName)),
      m_state(std::move(state)),
      m_location(std::move(location)),
      m_description(std::move(description)),
      m_class(std::move(devClass)),
      m_subClass(std::move(subClass)),
      m_type(std::move(devType)) {}

const std::string& Device::GetName() const noexcept { return m_logicalName; }
const std::string& Device::GetState() const noexcept { return m_state; }
const std::string& Device::GetLocation() const noexcept { return m_location; }
const std::string& Device::GetDescription() const noexcept { return m_description; }
const std::string& Device::GetClass() const noexcept { return m_class; }
const std::string& Device::GetSubClass() const noexcept { return m_subClass; }
const std::string& Device::GetType() const noexcept { return m_type; }

std::string Device::GetField(const std::string& fieldName) const {
    std::string f = StringUtil::ToLower(fieldName);
    if (f == "name") return m_logicalName;
    if (f == "status" || f == "state") return m_state;
    if (f == "location" || f == "loc") return m_location;
    if (f == "description" || f == "desc") return m_description;
    if (f == "class") return m_class;
    if (f == "subclass") return m_subClass;
    if (f == "type") return m_type;
    return "";
}

std::string SetupApiDeviceEnumerator::GetDeviceProperty(HDEVINFO devInfo, SP_DEVINFO_DATA& devData, DWORD property) {
    DWORD requiredSize = 0;
    SetupDiGetDeviceRegistryPropertyW(devInfo, &devData, property, nullptr, nullptr, 0, &requiredSize);
    if (requiredSize == 0) return {};

    std::vector<BYTE> buffer(requiredSize);
    if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devData, property, nullptr, buffer.data(), requiredSize, nullptr)) {
        return StringUtil::WideToAnsi(reinterpret_cast<const wchar_t*>(buffer.data()));
    }
    return {};
}

std::string SetupApiDeviceEnumerator::MapClassToPrefix(const std::string& className) {
    std::string c = StringUtil::ToLower(className);
    if (c == "diskdrive") return "hdisk";
    if (c == "net") return "ent";
    if (c == "display") return "disp";
    if (c == "processor") return "proc";
    if (c == "system") return "sys";
    if (c == "usb" || c == "usbdevice") return "usb";
    if (c == "scsiadapter" || c == "hdcontroller") return "scsi";
    if (c == "keyboard") return "kbd";
    if (c == "mouse") return "mouse";
    if (c == "media" || c == "audioendpoint") return "audio";
    return "dev";
}

std::vector<Device> SetupApiDeviceEnumerator::EnumerateDevices(bool includeNonPresent) {
    std::vector<Device> devices;
    std::map<std::string, int> classPrefixCounters;

    DWORD flags = DIGCF_ALLCLASSES;
    if (!includeNonPresent) {
        flags |= DIGCF_PRESENT;
    }

    HDEVINFO devInfo = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, flags);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return devices;
    }

    auto cleanup = [](HDEVINFO* h) { if (*h != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(*h); };
    std::unique_ptr<HDEVINFO, decltype(cleanup)> devInfoPtr(&devInfo, cleanup);

    SP_DEVINFO_DATA devData{};
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devData); ++i) {
        std::string className = GetDeviceProperty(devInfo, devData, SPDRP_CLASS);
        std::string desc = GetDeviceProperty(devInfo, devData, SPDRP_FRIENDLYNAME);
        if (desc.empty()) {
            desc = GetDeviceProperty(devInfo, devData, SPDRP_DEVICEDESC);
        }
        if (desc.empty()) continue;

        std::string enumerator = GetDeviceProperty(devInfo, devData, SPDRP_ENUMERATOR_NAME);
        std::string location = GetDeviceProperty(devInfo, devData, SPDRP_LOCATION_INFORMATION);
        if (location.empty()) {
            location = GetDeviceProperty(devInfo, devData, SPDRP_LOCATION_PATHS);
        }
        if (location.find('#') != std::string::npos) {
            location = location.substr(0, location.find('#'));
        }

        ULONG status = 0, problem = 0;
        std::string state = "Available";
        if (CR_SUCCESS == CM_Get_DevNode_Status(&status, &problem, devData.DevInst, 0)) {
            if (status & DN_HAS_PROBLEM) {
                state = (problem == CM_PROB_DISABLED) ? "Defined" : "Stopped";
            } else if (!(status & DN_STARTED)) {
                state = "Defined";
            }
        } else {
            state = "Defined";
        }

        std::string prefix = MapClassToPrefix(className);
        int idx = classPrefixCounters[prefix]++;
        std::string logicalName = prefix + std::to_string(idx);

        std::string subClass = enumerator.empty() ? "sys" : enumerator;
        std::string devType = className.empty() ? "system" : className;

        devices.emplace_back(logicalName, state, location, desc, className, subClass, devType);
    }

    return devices;
}

DeviceFilter::DeviceFilter(const CommandLineOptions& options) : m_options(options) {}

bool DeviceFilter::IncludesNonPresentDevices() const {
    return !m_options.filterState.empty() &&
           StringUtil::ToLower(m_options.filterState) != "available";
}

bool DeviceFilter::Matches(const Device& device) const {
    if (!m_options.filterName.empty() &&
        StringUtil::ToLower(device.GetName()) != StringUtil::ToLower(m_options.filterName))
        return false;
    if (!m_options.filterClass.empty() &&
        StringUtil::ToLower(device.GetClass()) != StringUtil::ToLower(m_options.filterClass))
        return false;
    if (!m_options.filterSubclass.empty() &&
        StringUtil::ToLower(device.GetSubClass()) != StringUtil::ToLower(m_options.filterSubclass))
        return false;
    if (!m_options.filterType.empty() &&
        StringUtil::ToLower(device.GetType()) != StringUtil::ToLower(m_options.filterType))
        return false;
    if (!m_options.filterState.empty() &&
        StringUtil::ToLower(device.GetState()) != StringUtil::ToLower(m_options.filterState))
        return false;
    return true;
}

std::vector<Device> DeviceFilter::Apply(const std::vector<Device>& devices) const {
    std::vector<Device> filtered;
    filtered.reserve(devices.size());

    for (const auto& device : devices) {
        if (Matches(device)) filtered.push_back(device);
    }
    return filtered;
}
