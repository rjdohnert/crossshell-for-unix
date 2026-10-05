#include "lscfg_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <algorithm>

int LscfgApplication::Run(int argc, char* argv[]) {
    LscfgOptions opts;
    if (!opts.Parse(argc, argv)) {
        return 0; // Help or version already shown
    }

    WmiEngine wmi;
    if (!wmi.IsInitialized()) {
        std::cerr << "Error: Failed to initialize Windows WMI COM interface.\n";
        return 1;
    }

    auto devices = InventoryCollector::Collect(wmi);

    // Apply Filter if Specified
    if (!opts.filter.empty()) {
        std::vector<HardwareDevice> filtered;
        for (const auto& dev : devices) {
            std::string resLower = dev.resourceName;
            std::transform(resLower.begin(), resLower.end(), resLower.begin(), ::tolower);
            if (dev.deviceClass == opts.filter || resLower == opts.filter) {
                filtered.push_back(dev);
            }
        }
        devices = filtered;
    }

    LscfgReporter::Report(devices, opts);
    return 0;
}
