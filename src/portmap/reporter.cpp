#include "reporter.hpp"

void PortmapReporter::Emit(const PortmapOptions& cfg, const std::vector<ConnectionEntry>& entries) {
    std::cout << "\n"
              << std::left
              << std::setw(8)  << "Proto"
              << std::setw(26) << "Local Address"
              << std::setw(26) << "Foreign Address"
              << std::setw(15) << "State";
    if (cfg.showProcess) {
        std::cout << std::setw(9)  << "PID"
                  << "Process Name";
    }
    std::cout << "\n";

    std::cout << std::string(cfg.showProcess ? 105 : 75, '-') << "\n";

    for (const auto& entry : entries) {
        std::cout << std::left
                  << std::setw(8)  << entry.proto
                  << std::setw(26) << entry.localAddr
                  << std::setw(26) << entry.foreignAddr
                  << std::setw(15) << (entry.state.empty() ? "-" : entry.state);

        if (cfg.showProcess) {
            std::cout << std::setw(9)  << entry.pid
                      << entry.processName;
        }
        std::cout << "\n";
    }

    std::cout << std::string(cfg.showProcess ? 105 : 75, '-') << "\n";
    std::cout << "Total active entries listed: " << entries.size() << "\n\n";
}
