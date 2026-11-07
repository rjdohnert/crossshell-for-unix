#include "errpt_parser.hpp"

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {

bool ParseCount(std::wstring_view text, DWORD& count) {
    if (text.empty()) return false;
    unsigned long long parsed = 0;
    for (wchar_t character : text) {
        if (character < L'0' || character > L'9') return false;
        const unsigned int digit = static_cast<unsigned int>(character - L'0');
        if (parsed > ((std::numeric_limits<unsigned long long>::max)() -
                      digit) / 10) {
            return false;
        }
        parsed = parsed * 10 + digit;
    }
    if (parsed == 0 ||
        parsed > (std::numeric_limits<DWORD>::max)()) {
        return false;
    }
    count = static_cast<DWORD>(parsed);
    return true;
}

bool ResolveLog(std::wstring_view value, std::wstring& logName) {
    std::wstring normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](wchar_t character) {
            return static_cast<wchar_t>(towlower(character));
        });
    if (normalized == L"system") logName = L"System";
    else if (normalized == L"application" || normalized == L"app") {
        logName = L"Application";
    } else if (normalized == L"security" || normalized == L"sec") {
        logName = L"Security";
    } else {
        return false;
    }
    return true;
}

bool SetOutputFormat(OutputFormat requested, bool& formatWasSet,
                     ErrptOptions& options, std::wstring& error) {
    if (formatWasSet && options.outputFormat != requested) {
        error = L"choose only one of --table, --json, or --csv.";
        return false;
    }
    options.outputFormat = requested;
    formatWasSet = true;
    return true;
}

} // namespace

ParseResult ParseArguments(int argc, wchar_t* argv[]) {
    ParseResult result;
    bool formatWasSet = false;

    for (int index = 1; index < argc; ++index) {
        if (!argv[index]) {
            result.error = L"received a null command-line argument.";
            return result;
        }
        const std::wstring_view argument(argv[index]);
        if (argument == L"-h" || argument == L"--help") {
            result.options.showHelp = true;
            result.success = true;
            return result;
        }
        if (argument == L"-v" || argument == L"--version") {
            result.options.showVersion = true;
            result.success = true;
            return result;
        }
        if (argument == L"-a" || argument == L"--all") {
            result.options.showAll = true;
            continue;
        }
        if (argument == L"-l" || argument == L"--log") {
            if (index + 1 >= argc || !argv[index + 1] ||
                !ResolveLog(argv[index + 1], result.options.logName)) {
                result.error = L"--log requires system, application, or "
                               L"security.";
                return result;
            }
            ++index;
            continue;
        }
        if (argument.rfind(L"--log=", 0) == 0) {
            if (!ResolveLog(argument.substr(6), result.options.logName)) {
                result.error = L"unsupported event log; choose system, "
                               L"application, or security.";
                return result;
            }
            continue;
        }
        if (argument == L"-n" || argument == L"--lines") {
            if (index + 1 >= argc || !argv[index + 1] ||
                !ParseCount(argv[index + 1], result.options.maxEntries)) {
                result.error = L"--lines requires a positive decimal count "
                               L"within the supported range.";
                return result;
            }
            ++index;
            continue;
        }
        if (argument.rfind(L"--lines=", 0) == 0) {
            if (!ParseCount(argument.substr(8), result.options.maxEntries)) {
                result.error = L"--lines requires a positive decimal count "
                               L"within the supported range.";
                return result;
            }
            continue;
        }
        if (argument == L"--json" || argument == L"--output=json") {
            if (!SetOutputFormat(OutputFormat::Json, formatWasSet,
                                 result.options, result.error)) return result;
            continue;
        }
        if (argument == L"--csv" || argument == L"--output=csv") {
            if (!SetOutputFormat(OutputFormat::Csv, formatWasSet,
                                 result.options, result.error)) return result;
            continue;
        }
        if (argument == L"--table" || argument == L"--output=table") {
            if (!SetOutputFormat(OutputFormat::Table, formatWasSet,
                                 result.options, result.error)) return result;
            continue;
        }
        if (argument == L"--output") {
            if (index + 1 >= argc || !argv[index + 1]) {
                result.error = L"--output requires table, json, or csv.";
                return result;
            }
            const std::wstring_view value(argv[++index]);
            OutputFormat requested;
            if (value == L"json") requested = OutputFormat::Json;
            else if (value == L"csv") requested = OutputFormat::Csv;
            else if (value == L"table") requested = OutputFormat::Table;
            else {
                result.error = L"unsupported output format; choose table, "
                               L"json, or csv.";
                return result;
            }
            if (!SetOutputFormat(requested, formatWasSet,
                                 result.options, result.error)) return result;
            continue;
        }
        if (argument == L"--pipe") {
            if (!result.options.pipeCommand.empty()) {
                result.error = L"--pipe may be specified only once.";
                return result;
            }
            if (index + 1 >= argc || !argv[index + 1] ||
                argv[index + 1][0] == L'\0') {
                result.error = L"--pipe requires a command.";
                return result;
            }
            result.options.pipeCommand = argv[++index];
            continue;
        }

        result.error = L"unknown option: " + std::wstring(argument);
        return result;
    }

    result.success = true;
    return result;
}

void PrintHelp() {
    constexpr char help[] = R"(errpt(1)                 CrossShell for UNIX Reference Manual                    errpt(1)

NAME
    errpt - display Windows Event Log entries

SYNOPSIS
    errpt [OPTIONS]

OPTIONS
    -l, --log NAME        Select system, application, or security.
    -n, --lines COUNT     Limit output to COUNT entries (default: 50).
    -a, --all             Read all entries (output is streamed).
    --output MODE         Select table, json, or csv output.
    --table               Print an escaped table.
    --json                Emit a JSON array.
    --csv                 Emit CSV with spreadsheet-formula protection.
    --pipe COMMAND        Send output to COMMAND and check its exit status.
    -h, --help            Display this help.
    -v, --version         Display version information.

EXAMPLES
    errpt --json
    errpt --csv > system-events.csv
    errpt -l application --output table

EXIT STATUS
    0          Help, version, or successful event-log query.
    1          Event-log, output, or pipe-command failure.
    2          Invalid command-line options.

CrossShell for UNIX                                                        errpt(1)
)";
    for (std::size_t index = 0; index < sizeof(help) - 1; ++index) {
        if (help[index] == '\n' &&
            (index == 0 || help[index - 1] != '\r')) {
            std::cout.put('\r');
        }
        std::cout.put(help[index]);
    }
}

void PrintVersion() {
    std::cout << "errpt v1.1.0\r\n";
}
