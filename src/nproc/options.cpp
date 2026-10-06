#include "options.hpp"

void NprocOptions::printUsage(const char* programName) {
    std::cout
        << "Usage: " << programName << " [options]\n"
        << "Print the number of available processing units.\n\n"
        << "Options:\n"
        << "  --all                 print all installed processing units\n"
        << "  --ignore=N            subtract N units from the result\n"
        << "  -h, --help            display this help and exit\n"
        << "      --version         output version information and exit\n";
}

void NprocOptions::printVersion() {
    std::cout << "nproc 1.0.0\n";
}

bool NprocOptions::parseIgnoreValue(const std::string& text, unsigned long long& outValue) {
    if (text.empty()) return false;
    char* end = nullptr;
    unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    outValue = value;
    return true;
}

bool NprocOptions::parse(int argc, char* argv[], NprocOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            printUsage((argc > 0 && argv[0]) ? argv[0] : "nproc");
            std::exit(0);
        }
        if (arg == "--version") {
            printVersion();
            std::exit(0);
        }
        if (arg == "--all") {
            opts.includeAll = true;
            continue;
        }
        if (arg.rfind("--ignore=", 0) == 0) {
            if (!parseIgnoreValue(arg.substr(9), opts.ignore)) {
                std::cerr << "nproc: invalid --ignore value\n";
                return false;
            }
            continue;
        }

        std::cerr << "nproc: unknown option -- " << arg << "\n";
        return false;
    }
    return true;
}
