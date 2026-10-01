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
 * SINGLE FILE INDEX: uptime.cpp
 * ============================================================================
 * WinUptime - Object-Oriented System Uptime, Session & Load Average Reporter
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. UptimeOptions class (CLI parsing & flags)
 * 2. [SYSTEM LOAD & SESSION SAMPLER] ....... SystemMetricsSampler class (CPU load & WTS sessions)
 * 3. [UPTIME CALCULATION & FORMATTER] ...... UptimeCalculator, UptimeEngine classes
 * 4. [APPLICATION CONTROLLER] .............. UptimeApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <memory>

#pragma comment(lib, "wtsapi32.lib")

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class UptimeOptions {
public:
    bool pretty{false};
    bool since{false};

    static void printUsage() {
        std::cout << R"(uptime(1)               CrossShell for UNIX Reference Manual                 uptime(1)

    NAME
        uptime - tell how long the system has been running

    SYNOPSIS
        uptime [OPTIONS]

    DESCRIPTION
        Display how long the system has been running, the current time, the
        number of active users, and system load averages.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -p, --pretty
            Show uptime in pretty format.

        -s, --since
            Show when the system started.

        -h, --help
            Display this reference manual and exit.

        -V, -v, --version
            Display version information and exit.

    EXAMPLES
        uptime
            Display current uptime and system load.

        uptime -p
            Display human-readable uptime.

        uptime -s
            Display system boot time.

    CrossShell for UNIX                                                      uptime(1)
)";
    }

    static void printVersion() {
        std::cout << "uptime 1.0.0\n";
    }

    static bool parse(int argc, char* argv[], UptimeOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                break;
            } else if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                printUsage();
                std::exit(0);
            } else if (arg == "-V" || arg == "--version" || arg == "-v") {
                printVersion();
                std::exit(0);
            } else if (arg == "-p" || arg == "--pretty") {
                opts.pretty = true;
            } else if (arg == "-s" || arg == "--since") {
                opts.since = true;
            } else if (arg[0] == '-') {
                std::cerr << "uptime: invalid option '" << arg << "'\n";
                return false;
            } else {
                std::cerr << "uptime: unexpected argument '" << arg << "'\n";
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. SYSTEM LOAD & SESSION SAMPLER
// ============================================================================

class SystemMetricsSampler {
private:
    static unsigned long long fileTimeToULL(const FILETIME& ft) {
        return (unsigned long long)ft.dwLowDateTime | ((unsigned long long)ft.dwHighDateTime << 32);
    }

public:
    static double getCpuUtilization() {
        FILETIME idleTime1, kernelTime1, userTime1;
        FILETIME idleTime2, kernelTime2, userTime2;

        if (!GetSystemTimes(&idleTime1, &kernelTime1, &userTime1)) return 0.0;
        Sleep(100);
        if (!GetSystemTimes(&idleTime2, &kernelTime2, &userTime2)) return 0.0;

        unsigned long long idle1 = fileTimeToULL(idleTime1);
        unsigned long long kernel1 = fileTimeToULL(kernelTime1);
        unsigned long long user1 = fileTimeToULL(userTime1);

        unsigned long long idle2 = fileTimeToULL(idleTime2);
        unsigned long long kernel2 = fileTimeToULL(kernelTime2);
        unsigned long long user2 = fileTimeToULL(userTime2);

        unsigned long long idleDiff = idle2 - idle1;
        unsigned long long kernelDiff = kernel2 - kernel1;
        unsigned long long userDiff = user2 - user1;

        unsigned long long totalSys = kernelDiff + userDiff;
        if (totalSys == 0) return 0.0;

        unsigned long long activeSys = totalSys - idleDiff;
        return static_cast<double>(activeSys) / totalSys;
    }

    static int getActiveUserCount() {
        PWTS_SESSION_INFOW pSessionInfo = NULL;
        DWORD dwCount = 0;
        int activeUsers = 0;

        if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &dwCount)) {
            for (DWORD i = 0; i < dwCount; ++i) {
                if (pSessionInfo[i].SessionId != 0 && pSessionInfo[i].State == WTSActive) {
                    activeUsers++;
                }
            }
            WTSFreeMemory(pSessionInfo);
        }
        return activeUsers > 0 ? activeUsers : 1;
    }
};

// ============================================================================
// 3. UPTIME CALCULATION & FORMATTER
// ============================================================================

class UptimeCalculator {
public:
    static std::string getUptimeString() {
        ULONGLONG ms = GetTickCount64();
        ULONGLONG seconds = ms / 1000;
        ULONGLONG minutes = seconds / 60;
        ULONGLONG hours = minutes / 60;
        ULONGLONG days = hours / 24;

        minutes %= 60;
        hours %= 24;

        std::stringstream ss;
        ss << "up ";
        if (days > 0) {
            ss << days << (days == 1 ? " day, " : " days, ");
            ss << std::setw(2) << std::setfill('0') << hours << ":"
               << std::setw(2) << std::setfill('0') << minutes;
        } else if (hours > 0) {
            ss << hours << ":" << std::setw(2) << std::setfill('0') << minutes;
        } else {
            ss << minutes << (minutes == 1 ? " min" : " mins");
        }
        return ss.str();
    }

    static std::string getPrettyUptimeString() {
        ULONGLONG ms = GetTickCount64();
        ULONGLONG seconds = ms / 1000;
        ULONGLONG minutes = seconds / 60;
        ULONGLONG hours = minutes / 60;
        ULONGLONG days = hours / 24;

        minutes %= 60;
        hours %= 24;

        std::ostringstream ss;
        ss << "up ";
        if (days > 0) {
            ss << days << (days == 1 ? " day" : " days");
            if (hours > 0 || minutes > 0) {
                ss << ", ";
                if (hours > 0) {
                    ss << hours << (hours == 1 ? " hour" : " hours");
                }
                if (minutes > 0) {
                    if (hours > 0) ss << ", ";
                    ss << minutes << (minutes == 1 ? " min" : " mins");
                }
            }
        } else if (hours > 0) {
            ss << hours << (hours == 1 ? " hour" : " hours");
            if (minutes > 0) {
                ss << ", " << minutes << (minutes == 1 ? " min" : " mins");
            }
        } else if (minutes > 0) {
            ss << minutes << (minutes == 1 ? " min" : " mins");
        } else {
            ss << "0 min";
        }
        return ss.str();
    }

    static std::string getBootTimeString() {
        ULONGLONG ms = GetTickCount64();
        time_t now = time(nullptr);
        time_t bootTime = now - static_cast<time_t>(ms / 1000);

        tm ltm;
        localtime_s(&ltm, &bootTime);

        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ltm);
        return std::string(buf);
    }

    static std::string getCurrentTimeFormatted() {
        time_t now = time(nullptr);
        tm ltm;
        localtime_s(&ltm, &now);

        char buf[32];
        strftime(buf, sizeof(buf), "%I:%M %p", &ltm);

        std::string s(buf);
        if (!s.empty() && s[0] == '0') {
            s = s.substr(1);
        }
        return s;
    }
};

class UptimeEngine {
private:
    UptimeOptions options;

public:
    explicit UptimeEngine(UptimeOptions opts) : options(std::move(opts)) {}

    int execute() {
        if (options.since) {
            std::cout << "system up since " << UptimeCalculator::getBootTimeString() << "\n";
            return 0;
        }

        std::string timeStr = UptimeCalculator::getCurrentTimeFormatted();
        std::string uptimeStr = options.pretty ? UptimeCalculator::getPrettyUptimeString() : UptimeCalculator::getUptimeString();
        int userCount = SystemMetricsSampler::getActiveUserCount();
        double cpuLoad = SystemMetricsSampler::getCpuUtilization();

        std::cout << " " << timeStr << "  "
                  << uptimeStr << ",  "
                  << userCount << (userCount == 1 ? " user,  " : " users,  ")
                  << "load averages: "
                  << std::fixed << std::setprecision(2) << cpuLoad << ", "
                  << cpuLoad << ", "
                  << cpuLoad << "\n";

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class UptimeApp {
public:
    static int run(int argc, char* argv[]) {
        UptimeOptions options;
        if (!UptimeOptions::parse(argc, argv, options)) {
            return 1;
        }
        UptimeEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return UptimeApp::run(argc, argv);
}
