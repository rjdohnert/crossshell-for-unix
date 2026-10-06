#include "mtr_app.hpp"

MTR::MTR(MtrOptions opts) : options_(std::move(opts)) {}

bool MTR::setup() {
    if (!dns_.resolveHost(options_.target, targetAddr_)) {
        std::cerr << "MTR: Failed to resolve address for " << options_.target << "\n";
        return false;
    }

    char ipBuf[INET_ADDRSTRLEN]{};
    if (!inet_ntop(AF_INET, &targetAddr_, ipBuf, sizeof(ipBuf))) {
        std::cerr << "MTR: Failed to format the resolved IPv4 address\n";
        return false;
    }
    targetIpStr_ = ipBuf;

    prober_ = std::make_unique<IcmpEngine>(options_.packetSize);
    return true;
}

void MTR::execute() {
    TerminalScreenGuard screenGuard;
    std::cout << "\033[2J";

    size_t cycles = 0;
    unsigned int routeLength = options_.maxTtl;
    bool keepRunning = true;

    while (keepRunning) {
        if (!isPaused_) {
            cycles++;
            for (unsigned int ttl = 1; ttl <= routeLength; ++ttl) {
                processKeyboard(keepRunning);
                if (!keepRunning || isPaused_) break;

                if (hops_.size() < ttl) {
                    hops_.emplace_back(static_cast<uint8_t>(ttl));
                }

                MtrDisplay::render(options_, targetIpStr_, hops_, cycles, ttl, isPaused_);

                auto outcome = prober_->probe(targetAddr_, static_cast<uint8_t>(ttl), options_.timeoutMs);

                auto& currentHop = hops_[ttl - 1];
                if (outcome.success) {
                    currentHop.addResult(outcome.ip, outcome.rtt, outcome.isDestination);
                    if (options_.resolveDns && currentHop.hostname.empty()) {
                        currentHop.hostname = dns_.reverseLookup(outcome.ip);
                    }
                    if (outcome.isDestination) {
                        routeLength = ttl;
                    }
                } else {
                    currentHop.addTimeout();
                }

                MtrDisplay::render(options_, targetIpStr_, hops_, cycles, ttl, isPaused_);
            }
        }

        if (!keepRunning) break;

        if (options_.reportCycles > 0 && static_cast<int>(cycles) >= options_.reportCycles) {
            break;
        }

        auto sleepDuration = std::chrono::milliseconds(static_cast<long long>(options_.intervalSec * 1000.0));
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < sleepDuration) {
            processKeyboard(keepRunning);
            if (!keepRunning) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }

    MtrDisplay::render(options_, targetIpStr_, hops_, cycles, -1, isPaused_);
    std::cout << "\nTrace complete.\n";
}

void MTR::processKeyboard(bool& keepRunning) {
    while (_kbhit()) {
        int ch = _getch();
        switch (ch) {
        case 'q':
        case 'Q':
            keepRunning = false;
            break;
        case 'r':
        case 'R':
            for (auto& h : hops_) h.reset();
            break;
        case 'n':
        case 'N':
            options_.resolveDns = !options_.resolveDns;
            break;
        case 'p':
        case 'P':
            isPaused_ = !isPaused_;
            break;
        default:
            break;
        }
    }
}
