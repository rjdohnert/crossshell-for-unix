#pragma once

#include "lsbt.hpp"
#include <vector>
#include <istream>

class IBluetoothDataSource {
public:
    virtual ~IBluetoothDataSource() = default;
    virtual std::vector<BluetoothDevice> getDevices() = 0;
};

class WindowsBluetoothSource : public IBluetoothDataSource {
public:
    std::vector<BluetoothDevice> getDevices() override;
    static std::vector<BluetoothDevice> getFallbackDevices();
private:
    static std::string formatBthAddress(const BTH_ADDR& addr);
    static std::string decodeClassOfDevice(ULONG cod, std::string& outDriver);
    static std::string wideToUtf8(const WCHAR* wstr);
};

class PipelineStreamSource : public IBluetoothDataSource {
public:
    explicit PipelineStreamSource(std::istream& is);
    std::vector<BluetoothDevice> getDevices() override;
private:
    std::istream& inStream;
};
