/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * SINGLE FILE INDEX: ts.cpp
 * ============================================================================
 * WinTs - Object-Oriented Real-Time Timestamp Pipeline Utility for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. TsOptions class (modes, flags, precision)
 * 2. [TIMESTAMP FORMATTER ENGINE] .......... TimestampFormatter class (wall/elapsed/delta)
 * 3. [STRUCTURED STREAM REPORTER] .......... StructuredReporter class (JSON/CSV/Table/Pipe)
 * 4. [CORE PIPELINE ENGINE] ................ TsEngine class (stream reading & real-time flush)
 * 5. [APPLICATION CONTROLLER] .............. TsApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <algorithm>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

enum class TsMode {
    WallClock,           // Standard calendar timestamp
    ElapsedSinceStart,   // -s: Time elapsed since ts was started
    Incremental          // -i: Time elapsed since the previous line
};

class TsOptions {
public:
    static constexpr std::string_view VERSION = "1.2.0";

    TsMode mode{TsMode::WallClock};
    bool useUtc{false};               // -u
    bool isIso{false};                // -z, --iso
    int subsecondPrecision{-1};       // -1: default, 0: none, 3: ms, 6: us, 9: ns
    std::string customFormat;         // Custom strftime format
    bool flushEveryLine{true};        // Real-time line flushing
    bool timestampEmptyLines{true};
    int outputFormat{0};              // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;

    static void printVersion() {
        std::cout << "ts version " << VERSION << "\n"
                  << "A high-performance timestamping pipeline utility for Windows.\n";
    }

    static void printHelp(const char* exeName) {
        std::cout <<
R"(NAME
    ts - timestamp standard input lines for Windows

SYNOPSIS
    )" << exeName << R"( [OPTIONS] [FORMAT]
    <command> | )" << exeName << R"( [OPTIONS] [FORMAT]

DESCRIPTION
    ts prepends a timestamp to each line received from standard input (stdin)
    and writes the result to standard output (stdout).

OPTIONS
    -i, --incremental
        Prepend the elapsed time since the previous line (delta time).
    -s, --since-start
        Prepend the elapsed time since the program was launched.
    -u, --utc
        Format calendar timestamps using Universal Coordinated Time (UTC).
    -z, --iso
        Format calendar timestamps in strict ISO-8601 extended format.
    -m, --millis
        Include milliseconds (3 fractional digits).
    -u, --micros
        Include microseconds (6 fractional digits).
    -n, --nanos
        Include nanoseconds (9 fractional digits).
    --json, --csv, --table
        Format transformed output records.
    --pipe COMMAND
        Send formatted output through COMMAND.
    -h, --help
        Display this help message.
    -V, --version
        Display version and author information.

EXAMPLES
    ping 127.0.0.1 | ts
    build.bat | ts -s
    stream_sensor | ts -i -u "%.6s"
)";
    }

    static bool parse(int argc, char* argv[], TsOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-i" || arg == "--incremental") {
                opts.mode = TsMode::Incremental;
            } else if (arg == "-s" || arg == "--since-start") {
                opts.mode = TsMode::ElapsedSinceStart;
            } else if (arg == "-u" || arg == "--utc") {
                opts.useUtc = true;
            } else if (arg == "-z" || arg == "--iso") {
                opts.isIso = true;
            } else if (arg == "-m" || arg == "--millis") {
                opts.subsecondPrecision = 3;
            } else if (arg == "--micros") {
                opts.subsecondPrecision = 6;
            } else if (arg == "-n" || arg == "--nanos") {
                opts.subsecondPrecision = 9;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (!arg.empty() && arg[0] != '-') {
                opts.customFormat = arg;
            } else if (arg.length() > 1 && arg[0] == '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    switch (c) {
                        case 'i': opts.mode = TsMode::Incremental; break;
                        case 's': opts.mode = TsMode::ElapsedSinceStart; break;
                        case 'u': opts.useUtc = true; break;
                        case 'z': opts.isIso = true; break;
                        case 'm': opts.subsecondPrecision = 3; break;
                        case 'n': opts.subsecondPrecision = 9; break;
                        default:
                            std::cerr << "ts: unrecognized option -- '" << c << "'\n";
                            return false;
                    }
                }
            }
        }
        return true;
    }
};

// ============================================================================
// 2. TIMESTAMP FORMATTER ENGINE
// ============================================================================

class TimestampFormatter {
private:
    TsOptions options;
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point lastTime;

public:
    explicit TimestampFormatter(TsOptions opts)
        : options(std::move(opts)),
          startTime(std::chrono::high_resolution_clock::now()),
          lastTime(startTime) {}

    std::string generatePrefix() {
        auto now = std::chrono::high_resolution_clock::now();

        if (options.mode == TsMode::ElapsedSinceStart) {
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime);
            return formatDuration(elapsed.count());
        } else if (options.mode == TsMode::Incremental) {
            auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime);
            lastTime = now;
            return formatDuration(delta.count());
        }

        // WallClock Mode
        auto sysNow = std::chrono::system_clock::now();
        auto sysTime = std::chrono::system_clock::to_time_t(sysNow);
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(sysNow.time_since_epoch()).count() % 1000000000;

        std::tm tmVal{};
        if (options.useUtc) {
            gmtime_s(&tmVal, &sysTime);
        } else {
            localtime_s(&tmVal, &sysTime);
        }

        if (options.isIso) {
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmVal);
            std::ostringstream oss;
            oss << buf;
            if (options.subsecondPrecision > 0) {
                oss << "." << formatSubseconds(nanos, options.subsecondPrecision);
            }
            oss << (options.useUtc ? "Z" : "");
            return oss.str();
        }

        if (!options.customFormat.empty()) {
            char buf[256];
            std::strftime(buf, sizeof(buf), options.customFormat.c_str(), &tmVal);
            return std::string(buf);
        }

        char buf[64];
        std::strftime(buf, sizeof(buf), "%b %d %H:%M:%S", &tmVal);
        std::ostringstream oss;
        oss << buf;
        int prec = (options.subsecondPrecision >= 0) ? options.subsecondPrecision : 6;
        if (prec > 0) {
            oss << "." << formatSubseconds(nanos, prec);
        }
        return oss.str();
    }

private:
    static std::string formatSubseconds(int64_t nanos, int precision) {
        std::ostringstream oss;
        oss << std::setw(9) << std::setfill('0') << nanos;
        std::string s = oss.str();
        return s.substr(0, std::min<size_t>(precision, 9));
    }

    std::string formatDuration(int64_t totalNanos) const {
        int64_t totalSec = totalNanos / 1000000000;
        int64_t remNanos = totalNanos % 1000000000;

        int64_t days = totalSec / 86400;
        int64_t hours = (totalSec % 86400) / 3600;
        int64_t mins = (totalSec % 3600) / 60;
        int64_t secs = totalSec % 60;

        std::ostringstream oss;
        if (days > 0) {
            oss << days << "d ";
        }
        if (hours > 0 || days > 0) {
            oss << std::setw(2) << std::setfill('0') << hours << ":";
        }
        oss << std::setw(2) << std::setfill('0') << mins << ":"
            << std::setw(2) << std::setfill('0') << secs;

        int prec = (options.subsecondPrecision >= 0) ? options.subsecondPrecision : 6;
        if (prec > 0) {
            oss << "." << formatSubseconds(remNanos, prec);
        }
        return oss.str();
    }
};

// ============================================================================
// 3. STRUCTURED STREAM REPORTER
// ============================================================================

class StructuredReporter {
public:
    static std::string jsonQuote(const std::string& value) {
        std::string out = "\"";
        for (unsigned char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out += (c < 0x20) ? '?' : static_cast<char>(c);
        }
        return out + "\"";
    }

    static std::string csvQuote(const std::string& value) {
        std::string out = "\"";
        for (char c : value) {
            out += (c == '"') ? "\"\"" : std::string(1, c);
        }
        return out + "\"";
    }

    static int output(const std::vector<std::pair<std::string, std::string>>& records,
                      int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"events\":[";
            for (size_t i = 0; i < records.size(); ++i) {
                if (i > 0) text += ",";
                text += "{\"timestamp\":" + jsonQuote(records[i].first) + ",\"line\":" + jsonQuote(records[i].second) + "}";
            }
            text += "]}\n";
        } else if (format == 2) {
            text = "\"timestamp\",\"line\"\n";
            for (const auto& r : records) {
                text += csvQuote(r.first) + "," + csvQuote(r.second) + "\n";
            }
        } else if (format == 3) {
            text = "TIMESTAMP\tLINE\n--------------------\n";
            for (const auto& r : records) {
                text += r.first + "\t" + r.second + "\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::fwrite(text.data(), 1, text.size(), stdout);
        }
        return 0;
    }
};

// ============================================================================
// 4. CORE PIPELINE ENGINE
// ============================================================================

class TsEngine {
private:
    TsOptions options;
    TimestampFormatter formatter;

public:
    explicit TsEngine(TsOptions opts)
        : options(opts), formatter(std::move(opts)) {}

    int execute() {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        std::vector<std::pair<std::string, std::string>> structuredRecords;
        std::string line;

        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::string prefix = formatter.generatePrefix();

            if (options.outputFormat == 0 && options.pipeCommand.empty()) {
                std::cout << prefix << " " << line << "\n";
                if (options.flushEveryLine) {
                    std::cout.flush();
                }
            } else {
                structuredRecords.emplace_back(prefix, line);
            }
        }

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            return StructuredReporter::output(structuredRecords, options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class TsApp {
public:
    static int run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        TsOptions options;
        if (!TsOptions::parse(argc, argv, options)) {
            return 1;
        }

        TsEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return TsApp::run(argc, argv);
}