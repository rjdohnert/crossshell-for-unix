#pragma once

#include "lsdev.hpp"
#include "options.hpp"

class IDeviceRenderer {
public:
    virtual ~IDeviceRenderer() = default;
    virtual void ShowHelp() const = 0;
    virtual void Render(const std::vector<Device>& devices) const = 0;
};

class ConsoleDeviceRenderer final : public IDeviceRenderer {
public:
    explicit ConsoleDeviceRenderer(const CommandLineOptions& options);
    void ShowHelp() const override;
    void Render(const std::vector<Device>& devices) const override;
private:
    void RenderDevices(const std::vector<Device>& devices) const;
    void RenderPredefinedDevices(const std::vector<Device>& devices) const;
    void RenderColumnValues(const std::vector<Device>& devices, const std::string& column) const;
    static std::string DisplayValue(const std::string& value);
    static std::string FitColumn(const std::string& value, size_t width);
    void RenderFormatted(const std::vector<Device>& devices, const std::string& format) const;
    const CommandLineOptions& m_options;
};
