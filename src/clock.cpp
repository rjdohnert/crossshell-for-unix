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
 * SINGLE FILE INDEX: clock.cpp
 * ============================================================================
 * WinClock - Object-Oriented System Time & Date Formatter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. ClockOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... ClockReporter class (JSON/CSV/Table/Pipe)
 * 3. [TIME FORMATTER & CLOCK ENGINE] ....... TimeFormatter, ClockEngine classes
 * 4. [APPLICATION CONTROLLER] .............. ClockApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdio>
#include <sstream>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class ClockOptions {
public:
    bool utc{false};
    bool rfc2822{false};
    std::string customFormat;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage() {
        std::cout << R"(clock(1)                CrossShell for UNIX Reference Manual                  clock(1)

    NAME
        clock - report or format the system clock and date

    SYNOPSIS
        clock [OPTIONS] [+FORMAT]

    DESCRIPTION
        Display the current system time and date according to specified formatting
        options or standard time representations.

    OPTIONS
        -R, --rfc-2822
            Output RFC 2822 formatted timestamp.

        -u, --utc, --gmt
            Print Coordinated Universal Time (UTC/GMT).

        --json, --csv, --table
            Output timestamp records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        clock
            Display current local date and time.

        clock -u
            Display current UTC/GMT date and time.

        clock -R
            Display RFC 2822 formatted timestamp.

        clock "+%Y-%m-%d %H:%M:%S"
            Display custom formatted date and time.

        clock --json
            Display timestamp in JSON format.

    CrossShell for UNIX                                                    clock(1)
)";
    }

    static void printVersion() {
        std::cout << "clock 1.0.0\n";
    }

    static bool parse(int argc, char* argv[], ClockOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage();
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-u" || arg == "--utc" || arg == "--gmt") {
                opts.utc = true;
            } else if (arg == "-R" || arg == "--rfc-2822") {
                opts.rfc2822 = true;
            } else if (!arg.empty() && arg[0] == '+') {
                opts.customFormat = arg.substr(1);
            } else {
                std::cerr << "clock: unknown option: " << arg << "\n";
                printUsage();
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

struct ClockDisplay {
    std::string dateTime;
    std::string timeZone;
    std::string timeMode;
};

class ClockReporter {
public:
    static std::string escapeJson(const std::string& value) {
        std::string escaped;
        for (char ch : value) {
            if (ch == '\\' || ch == '"') {
                escaped += '\\';
            }
            escaped += ch;
        }
        return escaped;
    }

    static int dispatch(const ClockDisplay& display, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"timestamp\":\"" + escapeJson(display.dateTime) +
                "\",\"timezone\":\"" + escapeJson(display.timeZone) +
                "\",\"time_mode\":\"" + escapeJson(display.timeMode) + "\"}\n";
        } else if (format == 2) {
            text = "timestamp,timezone,time_mode\n" + display.dateTime + "," +
                display.timeZone + "," + display.timeMode + "\n";
        } else if (format == 3) {
            text = "TIMESTAMP\n---------\n" + display.dateTime +
                "\nTIMEZONE\n--------\n" + display.timeZone +
                " (" + display.timeMode + ")\n";
        } else {
            text = display.dateTime + "\nTimezone: " + display.timeZone +
                " (" + display.timeMode + ")\n";
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. TIME FORMATTER & CLOCK ENGINE
// ============================================================================

class TimeFormatter {
public:
    static std::string formatDateTime(const std::tm& timeInfo, const ClockOptions& opts) {
        if (opts.rfc2822) {
            char buf[128];
            if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %z", &timeInfo) != 0) {
                return std::string(buf);
            }
        }

        if (!opts.customFormat.empty()) {
            char buf[256];
            if (std::strftime(buf, sizeof(buf), opts.customFormat.c_str(), &timeInfo) != 0) {
                return std::string(buf);
            }
        }

        std::ostringstream ss;
        ss << std::put_time(&timeInfo, "%A, %B %d, %Y %H:%M:%S");
        return ss.str();
    }

    static std::string wideToUtf8(const wchar_t* value) {
        if (value == nullptr || *value == L'\0') {
            return "";
        }
        int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (length <= 1) {
            return "";
        }
        std::string result(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], length, nullptr, nullptr);
        result.resize(static_cast<size_t>(length - 1));
        return result;
    }

    static void addTimeZoneInfo(ClockDisplay& display, const ClockOptions& opts) {
        if (opts.utc) {
            display.timeZone = "UTC";
            display.timeMode = "Standard Time";
            return;
        }

        TIME_ZONE_INFORMATION timeZoneInfo{};
        const DWORD timeZoneState = GetTimeZoneInformation(&timeZoneInfo);
        if (timeZoneState == TIME_ZONE_ID_DAYLIGHT) {
            display.timeZone = wideToUtf8(timeZoneInfo.DaylightName);
            display.timeMode = "Daylight Saving Time";
        } else if (timeZoneState == TIME_ZONE_ID_STANDARD) {
            display.timeZone = wideToUtf8(timeZoneInfo.StandardName);
            display.timeMode = "Standard Time";
        } else {
            display.timeZone = "Local Time";
            display.timeMode = "Standard Time";
        }

        if (display.timeZone.empty()) {
            display.timeZone = "Local Time";
        }
    }

    static ClockDisplay format(const std::tm& timeInfo, const ClockOptions& opts) {
        ClockDisplay display;
        display.dateTime = formatDateTime(timeInfo, opts);
        addTimeZoneInfo(display, opts);
        return display;
    }
};

class ClockEngine {
private:
    ClockOptions options;

public:
    explicit ClockEngine(ClockOptions opts) : options(std::move(opts)) {}

    int execute() {
        auto now = std::chrono::system_clock::now();
        std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
        std::tm timeInfo{};

#if defined(_MSC_VER)
        if (options.utc) {
            if (gmtime_s(&timeInfo, &nowTimeT) != 0) {
                std::cerr << "clock: error retrieving UTC time.\n";
                return 1;
            }
        } else if (localtime_s(&timeInfo, &nowTimeT) != 0) {
            std::cerr << "clock: error retrieving local time.\n";
            return 1;
        }
#else
        std::tm* tmPtr = options.utc ? std::gmtime(&nowTimeT) : std::localtime(&nowTimeT);
        if (tmPtr == nullptr) {
            std::cerr << "clock: error retrieving time.\n";
            return 1;
        }
        timeInfo = *tmPtr;
#endif

        ClockDisplay display = TimeFormatter::format(timeInfo, options);
        return ClockReporter::dispatch(display, options.outputFormat, options.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class ClockApp {
public:
    static int run(int argc, char* argv[]) {
        ClockOptions options;
        if (!ClockOptions::parse(argc, argv, options)) {
            return 1;
        }
        ClockEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return ClockApp::run(argc, argv);
}
