#include "lsusb_app.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <memory>
#include <utility>

LsusbApp::LsusbApp(CommandLineOptions opts) : options(std::move(opts)) {}

int LsusbApp::run() {
    std::unique_ptr<IUSBDataSource> source;
    if (options.readStdin) {
        source = std::make_unique<PipelineStreamSource>(std::cin);
    } else {
        source = std::make_unique<WindowsSetupApiSource>();
    }

    auto devices = source->getDevices();
    std::vector<USBDevice> filtered;
    for (const auto& dev : devices) {
        if (dev.matches(options.filterBus, options.filterDev, options.filterVid, options.filterPid)) {
            filtered.push_back(dev);
        }
    }

    std::unique_ptr<IOutputFormatter> formatter = FormatterFactory::create(options.format);
    std::cout << "\n" << formatter->render(filtered, options.verbose) << "\n";
    return 0;
}
