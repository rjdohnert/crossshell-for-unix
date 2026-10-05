#include "lsbt_app.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <memory>
#include <utility>

LsbtApp::LsbtApp(CommandLineOptions opts) : options(std::move(opts)) {}

int LsbtApp::run() {
    std::unique_ptr<IBluetoothDataSource> source;
    if (options.readStdin) {
        source = std::make_unique<PipelineStreamSource>(std::cin);
    } else {
        source = std::make_unique<WindowsBluetoothSource>();
    }

    auto devices = source->getDevices();
    std::vector<BluetoothDevice> filtered;
    for (const auto& dev : devices) {
        if (dev.matches(options.filterBus, options.filterSlot, options.filterMac, options.onlyConnected)) {
            filtered.push_back(dev);
        }
    }

    std::unique_ptr<IOutputFormatter> formatter = FormatterFactory::create(options.format);

    std::cout << "\n" << formatter->render(filtered, options.verbose, options.showDrivers) << "\n";
    return 0;
}
