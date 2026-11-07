/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "driverctl_scanner.hpp"

#include "driverctl_decoder.hpp"
#include "driverctl_utilities.hpp"

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <winsvc.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr DWORD kMaximumDevicePropertyBytes = 1024 * 1024;

class DeviceInfoSet {
public:
    explicit DeviceInfoSet(HDEVINFO handle) : handle_(handle) {}

    ~DeviceInfoSet() {
        if (handle_ != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(handle_);
    }

    HDEVINFO Get() const {
        return handle_;
    }

    bool Close() {
        if (handle_ == INVALID_HANDLE_VALUE) return true;
        HDEVINFO handle = handle_;
        handle_ = INVALID_HANDLE_VALUE;
        return SetupDiDestroyDeviceInfoList(handle) != FALSE;
    }

private:
    HDEVINFO handle_;
};

bool ReadDeviceProperty(HDEVINFO deviceInfo, SP_DEVINFO_DATA& deviceData,
                        const DEVPROPKEY& key, DEVPROPTYPE& propertyType,
                        std::vector<BYTE>& bytes) {
    DWORD requiredSize = 0;
    BOOL firstCall = SetupDiGetDevicePropertyW(
        deviceInfo, &deviceData, &key, &propertyType, nullptr, 0,
        &requiredSize, 0);
    if (!firstCall) {
        DWORD error = GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER || requiredSize == 0) return false;
    }
    if (requiredSize == 0 || requiredSize > kMaximumDevicePropertyBytes) return false;

    bytes.resize(requiredSize);
    DWORD actualSize = requiredSize;
    if (!SetupDiGetDevicePropertyW(deviceInfo, &deviceData, &key, &propertyType,
                                   bytes.data(), requiredSize, &actualSize, 0) ||
        actualSize > requiredSize) {
        bytes.clear();
        return false;
    }
    bytes.resize(actualSize);
    return true;
}

bool GetWidePropertyValue(const std::vector<BYTE>& bytes, std::wstring& value) {
    static_assert(sizeof(wchar_t) == 2, "Windows device properties use UTF-16.");
    if (bytes.empty() || bytes.size() % sizeof(wchar_t) != 0) return false;

    std::wstring wide(bytes.size() / sizeof(wchar_t), L'\0');
    std::memcpy(wide.data(), bytes.data(), bytes.size());
    auto terminator = std::find(wide.begin(), wide.end(), L'\0');
    if (terminator == wide.end()) return false;
    value.assign(wide.begin(), terminator);
    return true;
}

std::string GetDeviceStringProperty(HDEVINFO deviceInfo,
                                    SP_DEVINFO_DATA& deviceData,
                                    const DEVPROPKEY& key) {
    DEVPROPTYPE type = 0;
    std::vector<BYTE> bytes;
    std::wstring value;
    if (!ReadDeviceProperty(deviceInfo, deviceData, key, type, bytes) ||
        type != DEVPROP_TYPE_STRING || !GetWidePropertyValue(bytes, value)) {
        return {};
    }
    return ToUtf8(value);
}

std::wstring GetDeviceWideProperty(HDEVINFO deviceInfo,
                                   SP_DEVINFO_DATA& deviceData,
                                   const DEVPROPKEY& key) {
    DEVPROPTYPE type = 0;
    std::vector<BYTE> bytes;
    std::wstring value;
    if (!ReadDeviceProperty(deviceInfo, deviceData, key, type, bytes) ||
        type != DEVPROP_TYPE_STRING || !GetWidePropertyValue(bytes, value)) {
        return {};
    }
    return value;
}

std::vector<std::string> GetDeviceStringListProperty(
    HDEVINFO deviceInfo, SP_DEVINFO_DATA& deviceData, const DEVPROPKEY& key) {
    DEVPROPTYPE type = 0;
    std::vector<BYTE> bytes;
    if (!ReadDeviceProperty(deviceInfo, deviceData, key, type, bytes) ||
        type != DEVPROP_TYPE_STRING_LIST || bytes.empty() ||
        bytes.size() % sizeof(wchar_t) != 0) {
        return {};
    }

    std::wstring wide(bytes.size() / sizeof(wchar_t), L'\0');
    std::memcpy(wide.data(), bytes.data(), bytes.size());
    std::vector<std::string> values;
    size_t position = 0;
    while (position < wide.size() && wide[position] != L'\0') {
        size_t terminator = wide.find(L'\0', position);
        if (terminator == std::wstring::npos) return {};
        std::string value = ToUtf8(std::wstring_view(
            wide.data() + position, terminator - position));
        if (!value.empty()) values.push_back(std::move(value));
        position = terminator + 1;
    }
    return values;
}

bool ContainsCaseInsensitive(std::string_view text, std::string_view query) {
    if (query.empty()) return true;
    if (query.size() > text.size()) return false;
    for (size_t start = 0; start <= text.size() - query.size(); ++start) {
        size_t offset = 0;
        for (; offset < query.size(); ++offset) {
            unsigned char character = static_cast<unsigned char>(text[start + offset]);
            if (std::tolower(character) !=
                std::tolower(static_cast<unsigned char>(query[offset]))) {
                break;
            }
        }
        if (offset == query.size()) return true;
    }
    return false;
}

bool FilterContainsReflector(const std::vector<std::string>& filters) {
    return std::any_of(filters.begin(), filters.end(), [](const std::string& filter) {
        return ContainsCaseInsensitive(filter, "wudfrd");
    });
}

std::string DetermineDriverType(const std::wstring& service,
                                const std::vector<std::string>& upperFilters,
                                const std::vector<std::string>& lowerFilters) {
    if (service.empty()) return "No service associated";
    if (FilterContainsReflector(upperFilters) || FilterContainsReflector(lowerFilters)) {
        return "User-Mode Driver (UMDF Host)";
    }

    std::wstring keyPath = L"SYSTEM\\CurrentControlSet\\Services\\" + service;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return "Unknown (service registry lookup failed)";
    }

    DWORD type = 0;
    DWORD typeSize = sizeof(type);
    DWORD valueType = 0;
    LONG result = RegQueryValueExW(key, L"Type", nullptr, &valueType,
                                   reinterpret_cast<LPBYTE>(&type), &typeSize);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || valueType != REG_DWORD ||
        typeSize != sizeof(type)) {
        return "Unknown (service type unavailable)";
    }

    if (type & SERVICE_KERNEL_DRIVER) return "Kernel-Mode Driver";
    if (type & SERVICE_FILE_SYSTEM_DRIVER) return "File System Driver";
    if (type & (SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS)) {
        return "Win32 Service";
    }
    return "Unknown (unsupported service type)";
}

void RunDriverDiagnostics(DEVINST deviceInstance, DeviceRecord& record) {
    ULONG status = 0;
    ULONG problemCode = 0;
    CONFIGRET result = CM_Get_DevNode_Status(&status, &problemCode, deviceInstance, 0);
    if (result != CR_SUCCESS) {
        record.health = HealthStatus::Warning;
        record.diagnosticDetails = "Unable to query device node status flags.";
        return;
    }

    if (status & DN_HAS_PROBLEM) {
        record.problemCode = problemCode;
        record.health = problemCode == CM_PROB_DISABLED
            ? HealthStatus::Disabled : HealthStatus::Malfunction;
        record.diagnosticDetails = DescribeProblemCode(problemCode);
    } else if (!(status & DN_STARTED)) {
        record.health = HealthStatus::Healthy;
        record.diagnosticDetails =
            "Device node not started (no problem reported by Configuration Manager).";
    } else {
        record.health = HealthStatus::Healthy;
        record.diagnosticDetails =
            "Device node active; driver responsive and operating normally.";
    }
}

} // namespace

ScanResult DeviceScanner::Scan(bool testDrivers, std::string_view categoryFilter) {
    ScanResult result;
    HDEVINFO rawDeviceInfo = SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (rawDeviceInfo == INVALID_HANDLE_VALUE) {
        result.error = "SetupDiGetClassDevsW failed: " +
                       FormatWin32Error(GetLastError());
        return result;
    }
    DeviceInfoSet deviceInfo(rawDeviceInfo);

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA deviceData{};
        deviceData.cbSize = sizeof(deviceData);
        if (!SetupDiEnumDeviceInfo(deviceInfo.Get(), index, &deviceData)) {
            DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_ITEMS) {
                result.error = "SetupDiEnumDeviceInfo failed: " +
                               FormatWin32Error(error);
            }
            break;
        }

        DeviceRecord record;
        record.deviceName = GetDeviceStringProperty(
            deviceInfo.Get(), deviceData, DEVPKEY_Device_DeviceDesc);
        if (record.deviceName.empty()) {
            record.deviceName = GetDeviceStringProperty(
                deviceInfo.Get(), deviceData, DEVPKEY_Device_FriendlyName);
        }
        if (record.deviceName.empty()) record.deviceName = "Unknown Hardware Node";

        record.category = GetDeviceStringProperty(
            deviceInfo.Get(), deviceData, DEVPKEY_Device_Class);
        if (record.category.empty()) record.category = "System / Unspecified";
        if (!ContainsCaseInsensitive(record.category, categoryFilter)) continue;

        record.driverProvider = GetDeviceStringProperty(
            deviceInfo.Get(), deviceData, DEVPKEY_Device_DriverProvider);
        record.driverVersion = GetDeviceStringProperty(
            deviceInfo.Get(), deviceData, DEVPKEY_Device_DriverVersion);
        record.serviceName = GetDeviceWideProperty(
            deviceInfo.Get(), deviceData, DEVPKEY_Device_Service);
        record.upperFilters = GetDeviceStringListProperty(
            deviceInfo.Get(), deviceData, DEVPKEY_Device_UpperFilters);
        record.lowerFilters = GetDeviceStringListProperty(
            deviceInfo.Get(), deviceData, DEVPKEY_Device_LowerFilters);
        record.driverType = DetermineDriverType(
            record.serviceName, record.upperFilters, record.lowerFilters);
        if (testDrivers) RunDriverDiagnostics(deviceData.DevInst, record);
        result.devices.push_back(std::move(record));
    }

    if (!deviceInfo.Close() && result.error.empty()) {
        result.error = "SetupDiDestroyDeviceInfoList failed: " +
                       FormatWin32Error(GetLastError());
    }
    if (!result.error.empty()) result.devices.clear();
    return result;
}
