#pragma once

#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"

class LsDevApp {
public:
    LsDevApp(const CommandLineOptions& options,
             IDeviceEnumerator& enumerator,
             const DeviceFilter& filter,
             const IDeviceRenderer& renderer);
    int Run();
private:
    const CommandLineOptions& m_options;
    IDeviceEnumerator& m_enumerator;
    const DeviceFilter& m_filter;
    const IDeviceRenderer& m_renderer;
};
