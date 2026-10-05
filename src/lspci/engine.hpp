#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "lspci.hpp"
#include <vector>
#include <iostream>

class IPCIDataSource {
public:
    virtual ~IPCIDataSource() = default;
    virtual std::vector<PCIDevice> getDevices() = 0;
};

class WindowsPciSetupApiSource : public IPCIDataSource {
public:
    std::vector<PCIDevice> getDevices() override;
    static std::vector<PCIDevice> getFallbackTopology();
};

class PipelineStreamSource : public IPCIDataSource {
private:
    std::istream& inStream;
public:
    explicit PipelineStreamSource(std::istream& is);
    std::vector<PCIDevice> getDevices() override;
};

#endif // ENGINE_HPP
