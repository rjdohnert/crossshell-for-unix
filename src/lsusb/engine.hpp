#pragma once

#include "lsusb.hpp"
#include <vector>
#include <istream>

class IUSBDataSource {
public:
    virtual ~IUSBDataSource() = default;
    virtual std::vector<USBDevice> getDevices() = 0;
};

class WindowsSetupApiSource : public IUSBDataSource {
public:
    std::vector<USBDevice> getDevices() override;
    static std::vector<USBDevice> getFallbackDevices();
private:
    static std::string wideToUtf8(const std::wstring& wstr);
    static std::string getDeviceProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDevData, DWORD prop);
};

class PipelineStreamSource : public IUSBDataSource {
public:
    explicit PipelineStreamSource(std::istream& is);
    std::vector<USBDevice> getDevices() override;
private:
    std::istream& inStream;
};
