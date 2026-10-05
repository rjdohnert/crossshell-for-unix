#pragma once

#include "lsdev.hpp"
#include "options.hpp"

class IDeviceEnumerator {
public:
    virtual ~IDeviceEnumerator() = default;
    virtual std::vector<Device> EnumerateDevices(bool includeNonPresent) = 0;
};

class SetupApiDeviceEnumerator final : public IDeviceEnumerator {
public:
    std::vector<Device> EnumerateDevices(bool includeNonPresent) override;
private:
    static std::string GetDeviceProperty(HDEVINFO devInfo, SP_DEVINFO_DATA& devData, DWORD property);
    static std::string MapClassToPrefix(const std::string& className);
};

class DeviceFilter {
public:
    explicit DeviceFilter(const CommandLineOptions& options);
    bool IncludesNonPresentDevices() const;
    std::vector<Device> Apply(const std::vector<Device>& devices) const;
private:
    bool Matches(const Device& device) const;
    const CommandLineOptions& m_options;
};
