#ifndef MTR_OPTIONS_HPP
#define MTR_OPTIONS_HPP

#include "mtr.hpp"

struct MtrOptions {
    std::string target;
    uint8_t maxTtl{ 30 };
    DWORD timeoutMs{ 1000 };
    double intervalSec{ 1.0 };
    int reportCycles{ -1 };
    size_t packetSize{ 32 };
    bool resolveDns{ true };
    bool showHelp = false;
    bool showVersion = false;
};

class CliParser {
public:
    static void displayHelp(const char* progName = nullptr);
    static void displayVersion();
    static std::unique_ptr<MtrOptions> parse(int argc, char* argv[]);

private:
    static bool parseInteger(const char* text, long long minimum, long long maximum, long long& value);
    static bool parseInterval(const char* text, double& value);
};

#endif // MTR_OPTIONS_HPP
