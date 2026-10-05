#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <conio.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <cmath>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

static std::string sanitizeTerminalText(const std::string& text) {
    std::string sanitized;
    sanitized.reserve(text.size());
    for (unsigned char ch : text) {
        sanitized.push_back(ch >= 0x20 && ch != 0x7f ? static_cast<char>(ch) : '?');
    }
    return sanitized;
}

// ============================================================================
// 1. RAII Subsystem Initializers
// ============================================================================
class WinsockGuard {
public:
    WinsockGuard() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            throw std::runtime_error("Unable to initialize Winsock 2.2.");
        }
    }
    ~WinsockGuard() { WSACleanup(); }
};

class TerminalScreenGuard {
    HANDLE hOut_;
    DWORD originalMode_{ 0 };
public:
    TerminalScreenGuard() : hOut_(GetStdHandle(STD_OUTPUT_HANDLE)) {
        GetConsoleMode(hOut_, &originalMode_);
        // Enable VT100 / ANSI escape processing
        SetConsoleMode(hOut_, originalMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        // Hide terminal cursor
        std::cout << "\033[?25l";
    }
    ~TerminalScreenGuard() {
        // Restore cursor and original terminal state
        std::cout << "\033[?25h\033[0m";
        SetConsoleMode(hOut_, originalMode_);
    }
};

// ============================================================================
// 2. Domain Model: Statistics per Hop
// ============================================================================
class HopRecord {
public:
    uint8_t ttl{ 0 };
    std::string ip{ "???" };
    std::string hostname{ "" };
    bool reachedDestination{ false };

    size_t sent{ 0 };
    size_t received{ 0 };
    double lastRtt{ 0.0 };
    double minRtt{ 999999.0 };
    double maxRtt{ 0.0 };
    double sumRtt{ 0.0 };
    double sumSqRtt{ 0.0 };

    explicit HopRecord(uint8_t hopTtl = 0) : ttl(hopTtl) {}

    void addResult(const std::string& responderIp, double rttMs, bool isDestination) {
        sent++;
        received++;
        if (ip != responderIp) {
            hostname.clear();
        }
        ip = responderIp;
        lastRtt = rttMs;
        minRtt = (std::min)(minRtt, rttMs);
        maxRtt = (std::max)(maxRtt, rttMs);
        sumRtt += rttMs;
        sumSqRtt += (rttMs * rttMs);
        reachedDestination = isDestination;
    }

    void addTimeout() {
        sent++;
    }

    void reset() {
        sent = 0;
        received = 0;
        lastRtt = 0.0;
        minRtt = 999999.0;
        maxRtt = 0.0;
        sumRtt = 0.0;
        sumSqRtt = 0.0;
    }

    [[nodiscard]] double lossPercent() const noexcept {
        if (sent == 0) return 0.0;
        return (1.0 - (static_cast<double>(received) / static_cast<double>(sent))) * 100.0;
    }

    [[nodiscard]] double avgRtt() const noexcept {
        return (received > 0) ? (sumRtt / received) : 0.0;
    }

    [[nodiscard]] double bestRtt() const noexcept {
        return (received > 0) ? minRtt : 0.0;
    }

    [[nodiscard]] double stdDev() const noexcept {
        if (received <= 1) return 0.0;
        double avg = avgRtt();
        double variance = (sumSqRtt / received) - (avg * avg);
        return std::sqrt((std::max)(0.0, variance));
    }
};

// ============================================================================
// 3. DNS Service & Cache
// ============================================================================
class DnsService {
    std::unordered_map<std::string, std::string> cache_;
public:
    bool resolveHost(const std::string& host, IN_ADDR& addr) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_RAW;

        addrinfo* result = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) {
            return false;
        }

        addr = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
        freeaddrinfo(result);
        return true;
    }

    std::string reverseLookup(const std::string& ipStr) {
        auto it = cache_.find(ipStr);
        if (it != cache_.end()) {
            return it->second;
        }

        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        if (inet_pton(AF_INET, ipStr.c_str(), &(sa.sin_addr)) != 1) {
            cache_[ipStr] = "";
            return "";
        }

        char hostBuf[NI_MAXHOST];
        if (getnameinfo(reinterpret_cast<sockaddr*>(&sa), sizeof(sa),
                        hostBuf, sizeof(hostBuf), nullptr, 0, NI_NAMEREQD) == 0) {
            cache_[ipStr] = hostBuf;
            return hostBuf;
        }

        cache_[ipStr] = "";
        return "";
    }
};

// ============================================================================
// 4. ICMP Prober Engine
// ============================================================================
class IcmpEngine {
    HANDLE hIcmp_{ INVALID_HANDLE_VALUE };
    std::vector<char> sendBuffer_;
    std::vector<uint8_t> replyBuffer_;

public:
    explicit IcmpEngine(size_t packetSize) : sendBuffer_(packetSize, 'E') {
        hIcmp_ = IcmpCreateFile();
        if (hIcmp_ == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to obtain handle from IcmpCreateFile.");
        }
        size_t replySize = sizeof(ICMP_ECHO_REPLY) + packetSize + 64;
        replyBuffer_.resize(replySize);
    }

    ~IcmpEngine() {
        if (hIcmp_ != INVALID_HANDLE_VALUE) {
            IcmpCloseHandle(hIcmp_);
        }
    }

    struct ProbeOutcome {
        bool success{ false };
        bool isDestination{ false };
        std::string ip;
        double rtt{ 0.0 };
    };

    ProbeOutcome probe(IN_ADDR target, uint8_t ttl, DWORD timeoutMs) {
        IP_OPTION_INFORMATION options{};
        options.Ttl = ttl;

        auto start = std::chrono::high_resolution_clock::now();
        DWORD replies = IcmpSendEcho(
            hIcmp_,
            target.S_un.S_addr,
            sendBuffer_.data(),
            static_cast<WORD>(sendBuffer_.size()),
            &options,
            replyBuffer_.data(),
            static_cast<DWORD>(replyBuffer_.size()),
            timeoutMs
        );
        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        ProbeOutcome outcome;
        if (replies > 0) {
            auto* echoReply = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuffer_.data());
            IN_ADDR responder{};
            responder.S_un.S_addr = echoReply->Address;

            char ipStr[INET_ADDRSTRLEN]{};
            if (!inet_ntop(AF_INET, &responder, ipStr, sizeof(ipStr))) {
                return outcome;
            }
            outcome.ip = ipStr;
            outcome.rtt = (echoReply->RoundTripTime > 0) 
                        ? static_cast<double>(echoReply->RoundTripTime) 
                        : elapsedMs;

            if (echoReply->Status == IP_SUCCESS) {
                outcome.success = true;
                outcome.isDestination = true;
            } else if (echoReply->Status == IP_TTL_EXPIRED_TRANSIT) {
                outcome.success = true;
                outcome.isDestination = false;
            }
        }
        return outcome;
    }
};

// ============================================================================
// 5. Configuration & Command-Line Parser
// ============================================================================
struct MtrOptions {
    std::string target;
    uint8_t maxTtl{ 30 };
    DWORD timeoutMs{ 1000 };
    double intervalSec{ 1.0 };
    int reportCycles{ -1 }; // -1 indicates interactive mode
    size_t packetSize{ 32 };
    bool resolveDns{ true };
};

class CliParser {
    static bool parseInteger(const char* text, long long minimum, long long maximum,
                             long long& value) {
        try {
            size_t consumed = 0;
            long long parsed = std::stoll(text, &consumed, 10);
            if (consumed != std::string(text).size() || parsed < minimum || parsed > maximum) {
                return false;
            }
            value = parsed;
            return true;
        }
        catch (const std::exception&) {
            return false;
        }
    }

    static bool parseInterval(const char* text, double& value) {
        try {
            size_t consumed = 0;
            double parsed = std::stod(text, &consumed);
            if (consumed != std::string(text).size() || !std::isfinite(parsed) ||
                parsed < 0.01 || parsed > 86400.0) {
                return false;
            }
            value = parsed;
            return true;
        }
        catch (const std::exception&) {
            return false;
        }
    }

public:
    static void displayHelp(const char* progName) {
        std::cout << R"(mtr(1)                  CrossShell for UNIX Reference Manual                   mtr(1)

    NAME
        mtr - network diagnostic and traceroute tool (My Traceroute)

    SYNOPSIS
        mtr [OPTIONS] HOSTNAME_OR_IP

    DESCRIPTION
        mtr combines the functionality of traceroute and ping into a single
        network diagnostic tool. As mtr starts, it investigates the network
        connection between the host mtr runs on and a user-specified destination
        host.

    OPTIONS
        -c, --report-cycles COUNT
            Send COUNT pings to each hop, then print report and exit.

        -m, --max-ttl HOPS
            Set maximum hops / Time-To-Live (default: 30, max: 255).

        -i, --interval SECONDS
            Set interval between ping cycles (0.01-86400, default: 1.0).

        -t, --timeout MS
            ICMP reply timeout in milliseconds (1-60000, default: 1000).

        -s, --psize BYTES
            ICMP payload size in bytes (0-65500, default: 32).

        -n, --no-dns
            Do not resolve hostnames (display IP addresses only).

        -h, --help
            Display this reference manual.

        -v, --version
            Output version information and exit.

    INTERACTIVE COMMANDS
        q, Q
            Quit MTR.

        r, R
            Reset statistical counters.

        n, N
            Toggle reverse DNS name resolution.

        p, P
            Pause / resume dynamic probing.

    EXAMPLES
        mtr google.com
            Start interactive real-time trace to google.com.

        mtr -n -c 10 1.1.1.1
            Send 10 ping cycles without DNS resolution and print report.

        mtr --interval 0.5 --max-ttl 15 cloudflare.com
            Trace route with 500ms interval up to 15 hops.

    CrossShell for UNIX                                                    mtr(1)
)";
    }

    static std::unique_ptr<MtrOptions> parse(int argc, char* argv[]) {
        if (argc < 2) {
            return nullptr;
        }

        MtrOptions opts;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                displayHelp(argv[0]);
                exit(0);
            } else if (arg == "-v" || arg == "--version") {
                std::cout << "MTR v1.2.0\n";
                exit(0);
            } else if (arg == "-n" || arg == "--no-dns") {
                opts.resolveDns = false;
            } else if (arg == "-c" || arg == "--report-cycles") {
                long long value = 0;
                if (i + 1 >= argc || !parseInteger(argv[++i], 1, (std::numeric_limits<int>::max)(), value)) {
                    std::cerr << "MTR: Invalid report cycle count\n";
                    return nullptr;
                }
                opts.reportCycles = static_cast<int>(value);
            } else if (arg == "-m" || arg == "--max-ttl") {
                long long value = 0;
                if (i + 1 >= argc || !parseInteger(argv[++i], 1, 255, value)) {
                    std::cerr << "MTR: Maximum TTL must be between 1 and 255\n";
                    return nullptr;
                }
                opts.maxTtl = static_cast<uint8_t>(value);
            } else if (arg == "-i" || arg == "--interval") {
                if (i + 1 >= argc || !parseInterval(argv[++i], opts.intervalSec)) {
                    std::cerr << "MTR: Interval must be a finite value between 0.01 and 86400 seconds\n";
                    return nullptr;
                }
            } else if (arg == "-t" || arg == "--timeout") {
                long long value = 0;
                if (i + 1 >= argc || !parseInteger(argv[++i], 1, 60000, value)) {
                    std::cerr << "MTR: Timeout must be between 1 and 60000 milliseconds\n";
                    return nullptr;
                }
                opts.timeoutMs = static_cast<DWORD>(value);
            } else if (arg == "-s" || arg == "--psize") {
                long long value = 0;
                if (i + 1 >= argc || !parseInteger(argv[++i], 0, 65500, value)) {
                    std::cerr << "MTR: Packet size must be between 0 and 65500 bytes\n";
                    return nullptr;
                }
                opts.packetSize = static_cast<size_t>(value);
            } else if (arg[0] != '-') {
                opts.target = std::string(arg);
            } else {
                std::cerr << "MTR: Unknown parameter: " << arg << "\n";
                return nullptr;
            }
        }

        if (opts.target.empty()) {
            std::cerr << "MTR: Target hostname or IP address is required.\n";
            return nullptr;
        }

        return std::unique_ptr<MtrOptions>(new MtrOptions(opts));
    }
};

// ============================================================================
// 6. View: VT100 / Console Presenter
// ============================================================================
class MtrDisplay {
public:
    static void render(const MtrOptions& options,
                       const std::string& targetIp,
                       const std::vector<HopRecord>& hops,
                       size_t cycleCount,
                       int currentHop,
                       bool paused) {
        std::cout << "\033[H"; // Move cursor to row 1, col 1

        std::cout << "\033[1;37mMTR: " << sanitizeTerminalText(options.target) << " (" << targetIp << ")\033[0m"
                  << "  Count: " << cycleCount
                  << "  Interval: " << options.intervalSec << "s"
                  << "  Max TTL: " << static_cast<int>(options.maxTtl)
                  << "\033[K\n"; // \033[K clears to end of line

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
                      << "\033[K\n"; // \033[K clears to end of line
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
};

// ============================================================================
// 7. Controller: MtrApplication
// ============================================================================
class MTR {
    MtrOptions options_;
    IN_ADDR targetAddr_{};
    std::string targetIpStr_;
    DnsService dns_;
    std::unique_ptr<IcmpEngine> prober_;
    std::vector<HopRecord> hops_;
    bool isPaused_{ false };

public:
    explicit MTR(MtrOptions opts) : options_(std::move(opts)) {}

    bool setup() {
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

    void execute() {
        TerminalScreenGuard screenGuard;
        std::cout << "\033[2J"; // Clear entire screen initially

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
                            routeLength = ttl; // Lock down route boundaries
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

            // Sleep across interval duration while maintaining input responsiveness
            auto sleepDuration = std::chrono::milliseconds(static_cast<long long>(options_.intervalSec * 1000.0));
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start < sleepDuration) {
                processKeyboard(keepRunning);
                if (!keepRunning) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }

        // Final view
        MtrDisplay::render(options_, targetIpStr_, hops_, cycles, -1, isPaused_);
        std::cout << "\nTrace complete.\n";
    }

private:
    void processKeyboard(bool& keepRunning) {
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
};

// ============================================================================
// 8. Program Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    try {
        WinsockGuard winsock;

        auto opts = CliParser::parse(argc, argv);
        if (!opts) {
            CliParser::displayHelp(argv[0]);
            return 1;
        }

        MTR app(*opts);
        if (!app.setup()) {
            return 1;
        }

        app.execute();
    }
    catch (const std::exception& ex) {
        std::cerr << "MTR Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}