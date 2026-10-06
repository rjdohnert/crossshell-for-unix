#ifndef LTRACE_HPP
#define LTRACE_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <streambuf>
#include <cstdio>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "psapi.lib")

namespace Color {
    inline const std::string RESET     = "\033[0m";
    inline const std::string BOLD      = "\033[1m";
    inline const std::string BLUE      = "\033[1;34m";
    inline const std::string CYAN      = "\033[1;36m";
    inline const std::string GREEN     = "\033[1;32m";
    inline const std::string YELLOW    = "\033[1;33m";
    inline const std::string RED       = "\033[1;31m";
    inline const std::string MAGENTA   = "\033[1;35m";
    inline const std::string GRAY      = "\033[90m";
    inline const std::string B_CYAN    = "\033[96m";
}

struct Breakpoint {
    void* address;
    BYTE originalByte;
    std::string moduleName;
    std::string functionName;
    bool isArmed;
};

struct ThreadStepState {
    void* rearmAddress;
    std::vector<DWORD> suspendedThreadIds;
};

struct Config {
    DWORD targetPid = 0;
    std::string commandLine = "";
    std::vector<std::string> filterModules;
    std::string outputFile = "";
    bool followChildren = true;
    bool useColor = true;
    bool verbose = false;
    bool allExports = false;
    enum class OutputFormat { Human, Json, Csv, Table } outputFormat = OutputFormat::Human;
    std::string pipeCommand;
};

class PipeStreambuf : public std::streambuf {
private:
    FILE* pipe_;
    char buffer_[4096];

protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

    int sync() override {
        std::ptrdiff_t count = pptr() - pbase();
        if (count > 0 && std::fwrite(pbase(), 1, static_cast<size_t>(count), pipe_) != static_cast<size_t>(count)) return -1;
        setp(buffer_, buffer_ + sizeof(buffer_));
        return std::fflush(pipe_) == 0 ? 0 : -1;
    }

public:
    explicit PipeStreambuf(FILE* pipe) : pipe_(pipe) { setp(buffer_, buffer_ + sizeof(buffer_)); }
    ~PipeStreambuf() override { sync(); }
};

inline std::string jsonEscape(const std::string& value) {
    std::string out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) out.push_back('?');
            else out.push_back(static_cast<char>(ch));
            break;
        }
    }
    return out;
}

inline std::string csvEscape(const std::string& value) {
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') out += "\"\"";
        else out.push_back(ch);
    }
    return out + "\"";
}

inline bool initConsole() {
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode) != 0;
}

inline std::string toLower(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

#endif // LTRACE_HPP
