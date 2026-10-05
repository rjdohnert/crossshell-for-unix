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

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cctype>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <cstdio>
#include <sstream>
#include <windows.h>

// ============================================================================
// 1. DATA MODELS & UTILITY FUNCTIONS
// ============================================================================

enum class OutputFormat {
    None = 0,
    Json,
    Csv,
    Tsv,
    Table
};

struct SleepOptions {
    bool show_help = false;
    bool show_version = false;
    bool quiet = false;
    bool verbose = false;
    OutputFormat format = OutputFormat::None;
    std::wstring pipe_command;
    std::vector<std::wstring> raw_delays;
    double explicit_seconds = 0.0;
    bool has_explicit = false;
};

static std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

// Parses a single sleep argument (e.g., "1.5s", "10m", "2h", "1d", "500ms", "100us", or "5")
// and returns the duration in milliseconds.
static bool parse_sleep_argument(const std::wstring& arg, double& total_ms) {
    if (arg.empty()) return false;

    wchar_t* end_ptr = nullptr;
    double value = std::wcstod(arg.c_str(), &end_ptr);

    if (value < 0) {
        std::wcerr << L"sleep: bad delay value -- '" << arg << L"'\n";
        return false;
    }

    if (end_ptr == arg.c_str()) {
        std::wcerr << L"sleep: bad delay value -- '" << arg << L"'\n";
        return false;
    }

    double multiplier = 1000.0; // Default seconds -> ms
    if (*end_ptr != L'\0') {
        std::wstring suffix(end_ptr);
        for (auto& c : suffix) c = static_cast<wchar_t>(std::towlower(c));

        if (suffix == L"s" || suffix == L"sec" || suffix == L"secs") {
            multiplier = 1000.0;
        } else if (suffix == L"m" || suffix == L"min" || suffix == L"mins") {
            multiplier = 60.0 * 1000.0;
        } else if (suffix == L"h" || suffix == L"hr" || suffix == L"hrs" || suffix == L"hour" || suffix == L"hours") {
            multiplier = 3600.0 * 1000.0;
        } else if (suffix == L"d" || suffix == L"day" || suffix == L"days") {
            multiplier = 86400.0 * 1000.0;
        } else if (suffix == L"ms" || suffix == L"msec" || suffix == L"millis") {
            multiplier = 1.0;
        } else if (suffix == L"us" || suffix == L"usec" || suffix == L"micros") {
            multiplier = 0.001;
        } else {
            std::wcerr << L"sleep: unknown time unit -- '" << suffix << L"'\n";
            return false;
        }
    }

    total_ms += (value * multiplier);
    return true;
}

// ============================================================================
// 2. HELP & VERSION PRINTER
// ============================================================================

class SleepHelpSystem {
public:
    static void PrintHelp() {
        std::wcout << LR"(sleep(1)                CrossShell for UNIX Reference Manual                 sleep(1)

    NAME
        sleep - suspend execution for an interval of time

    SYNOPSIS
        sleep NUMBER[SUFFIX]...
        sleep [OPTIONS]

    DESCRIPTION
        sleep pauses process execution for the specified duration or the sum
        of all specified intervals. Each interval argument consists of a
        positive floating-point or integer number followed by an optional
        unit suffix. If no suffix is specified, seconds ('s') is assumed.

    QUALIFIERS AND UNITS
        s, sec, secs
            Seconds (default multiplier 1.0).

        m, min, mins
            Minutes (multiplier 60.0 seconds).

        h, hr, hrs, hours
            Hours (multiplier 3600.0 seconds).

        d, day, days
            Days (multiplier 86400.0 seconds).

        ms, msec, millis
            Milliseconds (multiplier 0.001 seconds).

        us, usec, micros
            Microseconds (multiplier 0.000001 seconds).

    OPTIONS
        -s, --seconds SECONDS
            Specify delay in seconds.

        -m, --minutes MINUTES
            Specify delay in minutes.

        -h, --hours HOURS
            Specify delay in hours.

        -d, --days DAYS
            Specify delay in days.

        --ms, --millis MILLIS
            Specify delay in milliseconds.

        -q, --quiet
            Suppress informational or warning messages.

        -v, --verbose
            Display timer start and countdown telemetry.

        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        sleep 5
            Pause execution for 5 seconds.

        sleep 1.5s 250ms
            Pause for 1.75 seconds total.

        sleep 10m
            Pause for 10 minutes.

        sleep 2h 30m
            Pause for 2.5 hours.

        sleep 3s --json
            Pause for 3 seconds and emit completion telemetry as JSON.

    CrossShell for UNIX                                                     sleep(1)
)";
    }

    static void PrintVersion() {
        std::wcout << L"sleep (CrossShell) 5.0.0\n"
                   << L"Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
    }
};

// ============================================================================
// 3. APPLICATION RUNNER
// ============================================================================

class SleepApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        SleepOptions options;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                options.show_help = true;
                continue;
            }
            if (arg == L"-V" || arg == L"-v" || arg == L"--version") {
                if (arg == L"-v" && argc > 2 && (i + 1 < argc || options.raw_delays.size() > 0 || options.has_explicit)) {
                    options.verbose = true;
                    continue;
                }
                options.show_version = true;
                continue;
            }
            if (arg == L"--verbose") {
                options.verbose = true;
                continue;
            }
            if (arg == L"-q" || arg == L"--quiet") {
                options.quiet = true;
                continue;
            }
            if (arg == L"--json" || arg == L"-j") {
                options.format = OutputFormat::Json;
                continue;
            }
            if (arg == L"--csv") {
                options.format = OutputFormat::Csv;
                continue;
            }
            if (arg == L"--tsv") {
                options.format = OutputFormat::Tsv;
                continue;
            }
            if (arg == L"--table") {
                options.format = OutputFormat::Table;
                continue;
            }
            if (arg == L"--output" && i + 1 < argc) {
                std::wstring fmt = argv[++i];
                if (fmt == L"json") options.format = OutputFormat::Json;
                else if (fmt == L"csv") options.format = OutputFormat::Csv;
                else if (fmt == L"tsv") options.format = OutputFormat::Tsv;
                else if (fmt == L"table") options.format = OutputFormat::Table;
                continue;
            }
            if (arg == L"--pipe" && i + 1 < argc) {
                options.pipe_command = argv[++i];
                continue;
            }
            if ((arg == L"-s" || arg == L"--seconds") && i + 1 < argc) {
                wchar_t* endp = nullptr;
                double val = std::wcstod(argv[++i], &endp);
                options.explicit_seconds += val;
                options.has_explicit = true;
                continue;
            }
            if ((arg == L"-m" || arg == L"--minutes") && i + 1 < argc) {
                wchar_t* endp = nullptr;
                double val = std::wcstod(argv[++i], &endp);
                options.explicit_seconds += (val * 60.0);
                options.has_explicit = true;
                continue;
            }
            if ((arg == L"--hours") && i + 1 < argc) {
                wchar_t* endp = nullptr;
                double val = std::wcstod(argv[++i], &endp);
                options.explicit_seconds += (val * 3600.0);
                options.has_explicit = true;
                continue;
            }
            if ((arg == L"-d" || arg == L"--days") && i + 1 < argc) {
                wchar_t* endp = nullptr;
                double val = std::wcstod(argv[++i], &endp);
                options.explicit_seconds += (val * 86400.0);
                options.has_explicit = true;
                continue;
            }
            if ((arg == L"--ms" || arg == L"--millis") && i + 1 < argc) {
                wchar_t* endp = nullptr;
                double val = std::wcstod(argv[++i], &endp);
                options.explicit_seconds += (val * 0.001);
                options.has_explicit = true;
                continue;
            }
            if (arg == L"--") {
                for (++i; i < argc; ++i) {
                    options.raw_delays.push_back(argv[i]);
                }
                break;
            }

            options.raw_delays.push_back(arg);
        }

        if (options.show_help) {
            SleepHelpSystem::PrintHelp();
            return 0;
        }

        if (options.show_version) {
            SleepHelpSystem::PrintVersion();
            return 0;
        }

        if (options.raw_delays.empty() && !options.has_explicit) {
            std::wcerr << L"usage: sleep NUMBER[SUFFIX]... or sleep [OPTIONS]\n"
                       << L"Try 'sleep --help' for more information.\n";
            return 1;
        }

        double total_sleep_ms = options.explicit_seconds * 1000.0;

        for (const auto& delay : options.raw_delays) {
            if (!parse_sleep_argument(delay, total_sleep_ms)) {
                return 1;
            }
        }

        if (total_sleep_ms < 0) total_sleep_ms = 0;

        if (options.verbose && !options.quiet) {
            std::wcout << L"sleep: sleeping for " << (total_sleep_ms / 1000.0) << L" seconds ("
                       << static_cast<long long>(total_sleep_ms) << L" ms)...\n";
        }

        long long ms_to_sleep = static_cast<long long>(total_sleep_ms);
        if (ms_to_sleep > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms_to_sleep));
        }

        if (options.format != OutputFormat::None || !options.pipe_command.empty()) {
            std::wstring text;
            if (options.format == OutputFormat::Json) {
                text = L"{\"status\":\"completed\",\"milliseconds\":" + std::to_wstring(ms_to_sleep) +
                       L",\"seconds\":" + std::to_wstring(total_sleep_ms / 1000.0) + L"}\n";
            } else if (options.format == OutputFormat::Csv) {
                text = L"status,milliseconds,seconds\ncompleted," + std::to_wstring(ms_to_sleep) +
                       L"," + std::to_wstring(total_sleep_ms / 1000.0) + L"\n";
            } else if (options.format == OutputFormat::Tsv) {
                text = L"status\tmilliseconds\tseconds\ncompleted\t" + std::to_wstring(ms_to_sleep) +
                       L"\t" + std::to_wstring(total_sleep_ms / 1000.0) + L"\n";
            } else {
                text = L"STATUS\tMILLISECONDS\tSECONDS\ncompleted\t" + std::to_wstring(ms_to_sleep) +
                       L"\t" + std::to_wstring(total_sleep_ms / 1000.0) + L"\n";
            }

            if (!options.pipe_command.empty()) {
                FILE* pipe = _wpopen(options.pipe_command.c_str(), L"w");
                if (pipe) {
                    std::string narrow = wide_to_utf8(text);
                    std::fwrite(narrow.data(), 1, narrow.size(), pipe);
                    _pclose(pipe);
                } else {
                    std::wcout << text;
                }
            } else {
                std::wcout << text;
            }
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    SleepApplication app;
    return app.Run(argc, argv);
}