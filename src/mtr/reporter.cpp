#include "reporter.hpp"

void MtrDisplay::render(const MtrOptions& options,
                        const std::string& targetIp,
                        const std::vector<HopRecord>& hops,
                        size_t cycleCount,
                        int currentHop,
                        bool paused) {
    std::cout << "\033[H";

    std::cout << "\033[1;37mMTR: " << sanitizeTerminalText(options.target) << " (" << targetIp << ")\033[0m"
              << "  Count: " << cycleCount
              << "  Interval: " << options.intervalSec << "s"
              << "  Max TTL: " << static_cast<int>(options.maxTtl)
              << "\033[K\n";

    std::cout << std::string(79, '=') << "\033[K\n";

    std::cout << std::left  << std::setw(40) << "Host"
              << std::right << std::setw(6)  << "Loss%"
              << std::setw(6)  << "Snt"
              << std::setw(7)  << "Last"
              << std::setw(7)  << "Avg"
              << std::setw(7)  << "Best"
              << std::setw(7)  << "Wrst"
              << std::setw(7)  << "StDev"
              << "\033[K\n";

    std::cout << std::string(79, '-') << "\033[K\n";

    for (const auto& hop : hops) {
        std::ostringstream label;
        label << std::setw(2) << static_cast<int>(hop.ttl) << ". ";
        if (hop.ip == "???") {
            label << "???";
        } else {
            if (options.resolveDns && !hop.hostname.empty()) {
                label << sanitizeTerminalText(hop.hostname) << " (" << hop.ip << ")";
            } else {
                label << hop.ip;
            }
        }

        std::string labelStr = label.str();
        if (labelStr.length() > 39) {
            labelStr = labelStr.substr(0, 36) + "...";
        }

        std::cout << std::left  << std::setw(40) << labelStr
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(5)  << hop.lossPercent() << "%"
                  << std::setw(6)  << hop.sent
                  << std::setw(7)  << hop.lastRtt
                  << std::setw(7)  << hop.avgRtt()
                  << std::setw(7)  << hop.bestRtt()
                  << std::setw(7)  << hop.maxRtt
                  << std::setw(7)  << hop.stdDev()
                  << "\033[K\n";
    }

    std::cout << std::string(79, '-') << "\033[K\n";

    if (paused) {
        std::cout << "[PAUSED] Probing suspended. Press 'p' to resume. \033[K\n";
    } else if (currentHop > 0) {
        std::cout << "Probing hop " << currentHop << "... [Press 'q' to stop] \033[K\n";
    } else {
        std::cout << "Cycle complete. Waiting for next interval... \033[K\n";
    }
}
