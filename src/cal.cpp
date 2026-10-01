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
 * SINGLE FILE INDEX: cal.cpp
 * ============================================================================
 * WinCal - Object-Oriented Calendar Utility for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. CalOptions class (CLI parsing & modes)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... CalReporter class (JSON/CSV/Table/Pipe)
 * 3. [CALENDAR GENERATION ENGINE] .......... CalendarCalculator and CalEngine classes
 * 4. [APPLICATION CONTROLLER] .............. CalApp class and main entry point
 * ============================================================================
 */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <string>
#include <vector>
#include <cctype>
#include <limits>
#include <cstdlib>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class CalOptions {
public:
    bool julian{false};
    bool mondayFirst{false};
    int month{-1};
    int year{-1};
    bool fullYear{false};
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* exe = nullptr) {
        (void)exe;
        std::cout << R"(cal(1)                  CrossShell for UNIX Reference Manual                  cal(1)

    NAME
        cal - display a calendar

    SYNOPSIS
        cal [OPTIONS] [MONTH] [YEAR]

    DESCRIPTION
        Display a calendar for the specified month or year. If no arguments
        are specified, the current month is displayed.

    OPTIONS
        -j, --julian
            Display Julian dates (days numbered 1 to 365/366).

        -m, --monday
            Display Monday as the first day of the week.

        -y, --year
            Display a calendar for the entire specified or current year.

        --json, --csv, --table
            Output calendar metadata in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send formatted output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        cal
            Display current month calendar.

        cal 12 2026
            Display calendar for December 2026.

        cal -y
            Display full calendar for the current year.

        cal --json
            Display current month metadata in JSON format.

    CrossShell for UNIX                                                    cal(1)
)";
    }

    static void printVersion() {
        std::cout << "cal 2.0.0\n";
    }

    static int parseMonth(const std::string& text) {
        if (text.empty()) return -1;
        try {
            int m = std::stoi(text);
            if (m >= 1 && m <= 12) return m;
        } catch (...) {}
        return -1;
    }

    static int parseYear(const std::string& text) {
        if (text.empty()) return -1;
        try {
            int y = std::stoi(text);
            if (y >= 1 && y <= 9999) return y;
        } catch (...) {}
        return -1;
    }

    static bool parse(int argc, char* argv[], CalOptions& opts) {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "-v" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-j" || arg == "--julian") {
                opts.julian = true;
            } else if (arg == "-m" || arg == "--monday") {
                opts.mondayFirst = true;
            } else if (arg == "-y" || arg == "--year") {
                opts.fullYear = true;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "cal: invalid option: " << arg << "\n";
                return false;
            } else {
                positional.push_back(arg);
            }
        }

        std::time_t t = std::time(nullptr);
        std::tm tmv{};
        localtime_s(&tmv, &t);
        int curMonth = tmv.tm_mon + 1;
        int curYear = tmv.tm_year + 1900;

        if (positional.empty()) {
            if (opts.fullYear) {
                opts.month = 0;
                opts.year = curYear;
            } else {
                opts.month = curMonth;
                opts.year = curYear;
            }
        } else if (positional.size() == 1) {
            int y = parseYear(positional[0]);
            if (y > 0) {
                opts.month = 0;
                opts.year = y;
                opts.fullYear = true;
            } else {
                int m = parseMonth(positional[0]);
                if (m > 0) {
                    opts.month = m;
                    opts.year = curYear;
                } else {
                    std::cerr << "cal: illegal argument: " << positional[0] << "\n";
                    return false;
                }
            }
        } else if (positional.size() >= 2) {
            opts.month = parseMonth(positional[0]);
            opts.year = parseYear(positional[1]);
            if (opts.month < 1 || opts.year < 1) {
                std::cerr << "cal: illegal month/year arguments\n";
                return false;
            }
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class CalReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"calendar\":\"" + content + "\"}\n";
        } else if (format == 2) {
            text = "calendar\n\"" + content + "\"\n";
        } else if (format == 3) {
            text = "CALENDAR\n--------\n" + content + "\n";
        } else {
            text = content;
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
// 3. CALENDAR GENERATION ENGINE
// ============================================================================

class CalendarCalculator {
public:
    static bool isLeapYear(int year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    static int getDaysInMonth(int month, int year) {
        static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (month == 2 && isLeapYear(year)) return 29;
        return (month >= 1 && month <= 12) ? days[month - 1] : 30;
    }

    static int getFirstDayOfMonth(int month, int year) {
        std::tm tmv{};
        tmv.tm_year = year - 1900;
        tmv.tm_mon = month - 1;
        tmv.tm_mday = 1;
        std::mktime(&tmv);
        return tmv.tm_wday; // 0 = Sunday
    }

    static std::string getMonthName(int month) {
        static const char* names[] = {
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        };
        return (month >= 1 && month <= 12) ? names[month - 1] : "";
    }

    static std::string renderMonth(int month, int year, bool mondayFirst) {
        std::ostringstream out;
        std::string title = getMonthName(month) + " " + std::to_string(year);
        int pad = (20 - static_cast<int>(title.size())) / 2;
        if (pad > 0) out << std::string(pad, ' ');
        out << title << "\n";

        if (mondayFirst) {
            out << "Mo Tu We Th Fr Sa Su\n";
        } else {
            out << "Su Mo Tu We Th Fr Sa\n";
        }

        int firstDay = getFirstDayOfMonth(month, year);
        if (mondayFirst) {
            firstDay = (firstDay + 6) % 7;
        }

        int daysInMonth = getDaysInMonth(month, year);

        for (int i = 0; i < firstDay; ++i) {
            out << "   ";
        }

        for (int d = 1; d <= daysInMonth; ++d) {
            out << std::setw(2) << d << " ";
            if ((firstDay + d) % 7 == 0) {
                out << "\n";
            }
        }
        out << "\n";
        return out.str();
    }
};

class CalEngine {
private:
    CalOptions options;

public:
    explicit CalEngine(CalOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::string content;
        if (options.fullYear || options.month == 0) {
            std::ostringstream ss;
            ss << "                             " << options.year << "\n\n";
            for (int m = 1; m <= 12; ++m) {
                ss << CalendarCalculator::renderMonth(m, options.year, options.mondayFirst) << "\n";
            }
            content = ss.str();
        } else {
            content = CalendarCalculator::renderMonth(options.month, options.year, options.mondayFirst);
        }

        return CalReporter::dispatch(content, options.outputFormat, options.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class CalApp {
public:
    static int run(int argc, char* argv[]) {
        CalOptions options;
        if (!CalOptions::parse(argc, argv, options)) {
            return 1;
        }
        CalEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return CalApp::run(argc, argv);
}
