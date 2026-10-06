#include "reporter.hpp"

void PrtconfReporter::report(const SystemConfigSummary& summary, bool showDevices) {
    std::cout << "System Model:        " << summary.systemModel << "\n";
    std::cout << "Node Name:           " << summary.nodeName << "\n";
    std::cout << "Kernel Architecture: " << summary.kernelArchitecture << "\n";
    std::cout << "Processors:          " << summary.numProcessors << "\n";
    std::cout << "Memory Size:         " << summary.totalMemoryMb << " MB\n";
    std::cout << "OS Level:            " << summary.osLevel << "\n";
    std::cout << "Devices Present:     " << summary.totalDevices << "\n";

    if (showDevices && !summary.classCounts.empty()) {
        std::cout << "\nDevice Class Summary:\n";
        std::cout << "--------------------  -----\n";
        for (const auto& it : summary.classCounts) {
            std::cout << "  " << std::setw(20) << std::left << it.first << " " << it.second << "\n";
        }
    }
}
