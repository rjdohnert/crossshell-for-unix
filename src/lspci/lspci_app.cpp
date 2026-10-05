#include "lspci_app.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <memory>

LspciApp::LspciApp(CommandLineOptions opts) : options(std::move(opts)) {}

int LspciApp::run() {
    std::unique_ptr<IPCIDataSource> source;
    if (options.readStdin) {
        source = std::make_unique<PipelineStreamSource>(std::cin);
    } else {
        source = std::make_unique<WindowsPciSetupApiSource>();
    }

    auto devices = source->getDevices();
    std::vector<PCIDevice> filtered;
    for (const auto& dev : devices) {
        if (dev.matches(options.filterDom, options.filterBus, options.filterSlot,
                        options.filterFunc, options.filterVid, options.filterDid)) {
            filtered.push_back(dev);
        }
    }

    auto formatter = FormatterFactory::create(options.format);
    std::cout << "\n" << formatter->render(filtered, options.verbose, options.showDrivers) << "\n";
    return 0;
}
