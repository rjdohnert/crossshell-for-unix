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
 * SINGLE FILE INDEX: cron.cpp
 * ============================================================================
 * WinCron - Object-Oriented Cron Task Scheduler & Daemon for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [CRON JOB & SCHEDULE SPECIFICATION] ... CronJob, CronTimeMatcher classes
 * 2. [OPTIONS & CONFIGURATION] ............. CronOptions class (CLI parsing & flags)
 * 3. [STRUCTURED OUTPUT REPORTER] .......... CronReporter class (JSON/CSV/Table/Pipe)
 * 4. [CRONTAB PARSER & SCHEDULER ENGINE] ... CrontabParser, CronEngine classes
 * 5. [APPLICATION CONTROLLER] .............. CronApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. CRON JOB & SCHEDULE SPECIFICATION
// ============================================================================

struct CronJob {
    std::string minutePattern;
    std::string hourPattern;
    std::string domPattern;
    std::string monthPattern;
    std::string dowPattern;
    std::string command;
    std::string rawLine;
};

class CronTimeMatcher {
private:
    static std::vector<std::string> split(const std::string& str, char delim) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delim)) {
            if (!token.empty()) tokens.push_back(token);
        }
        return tokens;
    }

    static bool matchSubfield(const std::string& sub, int val, int minVal, int maxVal) {
        std::string expr = sub;
        int step = 1;

        size_t slashPos = sub.find('/');
        if (slashPos != std::string::npos) {
            expr = sub.substr(0, slashPos);
            step = std::stoi(sub.substr(slashPos + 1));
            if (step <= 0) step = 1;
        }

        int rangeStart = minVal;
        int rangeEnd = maxVal;

        if (expr == "*") {
            rangeStart = minVal;
            rangeEnd = maxVal;
        } else {
            size_t dashPos = expr.find('-');
            if (dashPos != std::string::npos) {
                rangeStart = std::stoi(expr.substr(0, dashPos));
                rangeEnd = std::stoi(expr.substr(dashPos + 1));
            } else {
                rangeStart = std::stoi(expr);
                rangeEnd = rangeStart;
            }
        }

        if (val < rangeStart || val > rangeEnd) return false;
        return ((val - rangeStart) % step) == 0;
    }

public:
    static bool matchField(const std::string& pattern, int value, int minVal, int maxVal) {
        if (pattern == "*") return true;

        auto subfields = split(pattern, ',');
        for (const auto& sub : subfields) {
            if (matchSubfield(sub, value, minVal, maxVal)) {
                return true;
            }
        }
        return false;
    }

    static bool isJobDue(const CronJob& job, const struct tm& localTime) {
        int min = localTime.tm_min;
        int hour = localTime.tm_hour;
        int dom = localTime.tm_mday;
        int mon = localTime.tm_mon + 1;
        int dow = localTime.tm_wday; // 0 = Sunday

        bool minMatch = matchField(job.minutePattern, min, 0, 59);
        bool hourMatch = matchField(job.hourPattern, hour, 0, 23);
        bool domMatch = matchField(job.domPattern, dom, 1, 31);
        bool monMatch = matchField(job.monthPattern, mon, 1, 12);
        bool dowMatch = matchField(job.dowPattern, dow, 0, 6) ||
                        (dow == 0 && matchField(job.dowPattern, 7, 0, 7));

        return minMatch && hourMatch && domMatch && monMatch && dowMatch;
    }
};

// ============================================================================
// 2. OPTIONS & CONFIGURATION
// ============================================================================

class CronOptions {
public:
    std::string crontabPath{"crontab.txt"};
    bool forceSystemCrontab{false};
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* prog = nullptr) {
        (void)prog;
        std::cout << R"(cron(1)                 CrossShell for UNIX Reference Manual                  cron(1)

    NAME
        cron - run scheduled Windows crontab jobs

    SYNOPSIS
        cron [OPTIONS] [CRONTAB_PATH]

    DESCRIPTION
        Loads crontab.txt by default and evaluates five-field cron schedules.
        Blank lines and comments are ignored; wildcards, lists, ranges, and steps
        are supported.

    OPTIONS
        --system-crontab
            Store and read from C:\cron\crontab.txt.

        --json, --csv, --table
            Emit daemon status records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send daemon status directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

        --
            End options before CRONTAB_PATH.

    EXAMPLES
        cron
            Run cron daemon using default crontab.txt.

        cron C:\cron\production.txt
            Run cron daemon using specified crontab file.

        cron --system-crontab --json
            Run system crontab and emit JSON status.

    CrossShell for UNIX                                                    cron(1)
)";
    }

    static void printVersion() {
        std::cout << "cron 1.0.0\n";
    }

    static bool parse(int argc, char* argv[], CronOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }
            if (arg == "--help" || arg == "-h" || arg == "/?" || arg == "-?") {
                printUsage(argv[0]);
                std::exit(0);
            }
            if (arg == "--version") {
                printVersion();
                std::exit(0);
            }
            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    if (!opts.forceSystemCrontab) opts.crontabPath = argv[j];
                }
                break;
            }
            if (arg == "--system-crontab") {
                opts.forceSystemCrontab = true;
                continue;
            }

            if (opts.forceSystemCrontab) {
                std::cerr << "cron: custom crontab path is not allowed with --system-crontab. Required path: C:\\cron\\crontab.txt\n";
                return false;
            }

            opts.crontabPath = arg;
        }

        if (opts.forceSystemCrontab) {
            opts.crontabPath = "C:\\cron\\crontab.txt";
        }

        return true;
    }
};

// ============================================================================
// 3. STRUCTURED OUTPUT REPORTER
// ============================================================================

class CronReporter {
public:
    static int dispatchStatus(size_t jobCount, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"status\":\"running\",\"jobs\":" + std::to_string(jobCount) + "}\n";
        } else if (format == 2) {
            text = "status,jobs\nrunning," + std::to_string(jobCount) + "\n";
        } else if (format == 3) {
            text = "STATUS\tJOBS\nrunning\t" + std::to_string(jobCount) + "\n";
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else if (format != 0) {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 4. CRONTAB PARSER & SCHEDULER ENGINE
// ============================================================================

class CrontabParser {
public:
    static bool parseLine(const std::string& line, CronJob& job) {
        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return false;
        trimmed = trimmed.substr(start);

        if (trimmed.empty() || trimmed[0] == '#') return false;

        std::istringstream iss(trimmed);
        if (!(iss >> job.minutePattern >> job.hourPattern >> job.domPattern >> job.monthPattern >> job.dowPattern)) {
            return false;
        }

        std::string remainder;
        std::getline(iss, remainder);
        size_t cmdStart = remainder.find_first_not_of(" \t");
        if (cmdStart != std::string::npos) {
            job.command = remainder.substr(cmdStart);
        } else {
            return false;
        }

        job.rawLine = line;
        return true;
    }

    static std::vector<CronJob> load(const std::string& filepath) {
        std::vector<CronJob> jobs;
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return jobs;
        }

        std::string line;
        while (std::getline(file, line)) {
            CronJob job;
            if (parseLine(line, job)) {
                jobs.push_back(job);
            }
        }
        return jobs;
    }

    static FILETIME getFileLastWriteTime(const std::string& filepath) {
        FILETIME ft = { 0 };
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard, &fad)) {
            ft = fad.ftLastWriteTime;
        }
        return ft;
    }
};

static std::atomic<bool> g_cronRunning{true};

static BOOL WINAPI ConsoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[*] Shutting down Cron daemon..." << std::endl;
        g_cronRunning = false;
        return TRUE;
    }
    return FALSE;
}

class CronEngine {
private:
    CronOptions options;

    static void executeCommandAsync(const std::string& cmd) {
        std::thread([cmd]() {
            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi = { 0 };
            si.cb = sizeof(si);

            std::string fullCmd = "cmd.exe /c " + cmd;

            BOOL success = CreateProcessA(
                NULL, &fullCmd[0], NULL, NULL, FALSE,
                CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi
            );

            if (success) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            } else {
                std::cerr << "[-] Execution failed for: " << cmd << std::endl;
            }
        }).detach();
    }

public:
    explicit CronEngine(CronOptions opts) : options(std::move(opts)) {}

    int execute() {
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

        std::vector<CronJob> jobs = CrontabParser::load(options.crontabPath);

        if (options.outputFormat != 0 || !options.pipeCommand.empty()) {
            CronReporter::dispatchStatus(jobs.size(), options.outputFormat, options.pipeCommand);
        } else {
            std::cout << "\n";
            std::cout << " Cron Task Scheduler\n";
            std::cout << " Target Crontab File: " << options.crontabPath << "\n";
            std::cout << " Press Ctrl+C to stop.\n\n";
            std::cout << "[*] Loaded " << jobs.size() << " active cron job(s).\n";
        }

        FILETIME lastWriteTime = CrontabParser::getFileLastWriteTime(options.crontabPath);
        time_t lastExecutedMinute = 0;

        while (g_cronRunning) {
            FILETIME currentWriteTime = CrontabParser::getFileLastWriteTime(options.crontabPath);
            if (CompareFileTime(&lastWriteTime, &currentWriteTime) != 0) {
                lastWriteTime = currentWriteTime;
                jobs = CrontabParser::load(options.crontabPath);
                std::cout << "[*] Crontab modified. Reloaded " << jobs.size() << " job(s).\n";
            }

            time_t now = time(nullptr);
            time_t currentMinuteTimestamp = (now / 60) * 60;

            if (currentMinuteTimestamp > lastExecutedMinute) {
                lastExecutedMinute = currentMinuteTimestamp;

                struct tm localTime;
                localtime_s(&localTime, &now);

                char timeBuf[64];
                strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &localTime);

                for (const auto& job : jobs) {
                    if (CronTimeMatcher::isJobDue(job, localTime)) {
                        std::cout << "[" << timeBuf << "] [CRON EXEC] " << job.command << std::endl;
                        executeCommandAsync(job.command);
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        return 0;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class CronApp {
public:
    static int run(int argc, char* argv[]) {
        CronOptions options;
        if (!CronOptions::parse(argc, argv, options)) {
            return 1;
        }
        CronEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return CronApp::run(argc, argv);
}
