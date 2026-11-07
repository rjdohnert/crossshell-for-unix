#include "handles_cli.hpp"

#include <cstdint>
#include <iostream>
#include <limits>

namespace {
bool ParseUnsigned(const std::wstring& text, uint64_t maximum,
                   uint64_t& value) {
    if (text.empty()) return false;

    uint64_t parsed = 0;
    for (wchar_t character : text) {
        if (character < L'0' || character > L'9') return false;
        const uint64_t digit = static_cast<uint64_t>(character - L'0');
        if (parsed > (maximum - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    value = parsed;
    return true;
}

bool ParseFormat(const std::wstring& text, OutputFormat& format) {
    if (text == L"table") format = OutputFormat::table;
    else if (text == L"csv") format = OutputFormat::csv;
    else if (text == L"json") format = OutputFormat::json;
    else if (text == L"tsv") format = OutputFormat::tsv;
    else if (text == L"pipe") format = OutputFormat::pipe;
    else return false;
    return true;
}

bool ParseSortColumn(const std::wstring& text, SortColumn& column) {
    if (text == L"handles") column = SortColumn::handles;
    else if (text == L"pid") column = SortColumn::pid;
    else if (text == L"name") column = SortColumn::name;
    else return false;
    return true;
}

bool ReadValue(int& index, int argc, wchar_t* argv[], const wchar_t* option,
               std::wstring& value, std::wstring& error) {
    if (index + 1 >= argc || argv[index + 1] == nullptr ||
        argv[index + 1][0] == L'\0') {
        error = std::wstring(option) + L" requires a value";
        return false;
    }
    value = argv[++index];
    return true;
}
}

bool HandlesCli::parse(int argc, wchar_t* argv[], AppConfig& config,
                       std::wstring& error) {
    config = AppConfig{};
    error.clear();

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr || argv[i][0] == L'\0') {
            error = L"empty arguments are not valid";
            return false;
        }
        const std::wstring argument = argv[i];

        if (argument == L"-h" || argument == L"--help" ||
            argument == L"/?" || argument == L"-?") {
            config.showHelp = true;
            return true;
        }
        if (argument == L"-V" || argument == L"--version") {
            config.showVersion = true;
            return true;
        }
        if (argument == L"--json" || argument == L"-j") {
            config.format = OutputFormat::json;
            continue;
        }
        if (argument == L"--csv") {
            config.format = OutputFormat::csv;
            continue;
        }
        if (argument == L"--tsv") {
            config.format = OutputFormat::tsv;
            continue;
        }
        if (argument == L"--table") {
            config.format = OutputFormat::table;
            continue;
        }
        if (argument == L"--asc") {
            config.sortDescending = false;
            continue;
        }
        if (argument == L"--desc") {
            config.sortDescending = true;
            continue;
        }
        if (argument == L"--pipe") {
            if (!ReadValue(i, argc, argv, L"--pipe", config.pipeCommand, error)) {
                return false;
            }
            continue;
        }
        if (argument == L"-f" || argument == L"--format" ||
            argument == L"--output") {
            std::wstring value;
            if (!ReadValue(i, argc, argv, argument.c_str(), value, error)) {
                return false;
            }
            if (!ParseFormat(value, config.format)) {
                error = L"unknown output format: " + value;
                return false;
            }
            continue;
        }
        if (argument == L"-s" || argument == L"--sort") {
            std::wstring value;
            if (!ReadValue(i, argc, argv, argument.c_str(), value, error)) {
                return false;
            }
            if (!ParseSortColumn(value, config.sortBy)) {
                error = L"unknown sort column: " + value;
                return false;
            }
            continue;
        }
        if (argument == L"-n" || argument == L"--name") {
            if (!ReadValue(i, argc, argv, argument.c_str(),
                           config.filterName, error)) {
                return false;
            }
            continue;
        }
        if (argument == L"-p" || argument == L"--pid") {
            std::wstring value;
            if (!ReadValue(i, argc, argv, argument.c_str(), value, error)) {
                return false;
            }
            uint64_t parsed = 0;
            if (!ParseUnsigned(value, (std::numeric_limits<DWORD>::max)(),
                              parsed)) {
                error = L"PID must be an unsigned 32-bit integer";
                return false;
            }
            config.filterPid = static_cast<DWORD>(parsed);
            continue;
        }
        if (argument == L"-t" || argument == L"--top") {
            std::wstring value;
            if (!ReadValue(i, argc, argv, argument.c_str(), value, error)) {
                return false;
            }
            uint64_t parsed = 0;
            if (!ParseUnsigned(value, (std::numeric_limits<size_t>::max)(),
                              parsed)) {
                error = L"top count must be an unsigned integer";
                return false;
            }
            config.topN = static_cast<size_t>(parsed);
            continue;
        }

        if (!argument.empty() && argument[0] == L'-') {
            error = L"unknown option: " + argument;
        } else {
            error = L"unexpected positional argument: " + argument;
        }
        return false;
    }
    return true;
}

void HandlesCli::printHelp() {
    std::wcout << LR"(handles(1)              CrossShell for UNIX Reference Manual              handles(1)

NAME
    handles - inspect handle counts for running Windows processes

SYNOPSIS
    handles [OPTIONS]

OPTIONS
    -f, --format FORMAT, --output FORMAT
        Output format: table, csv, json, tsv, or pipe. Default: table.

    -s, --sort COLUMN
        Sort by handles, pid, or name. Default: handles.

    --asc, --desc
        Select ascending or descending order. Default: descending.

    -n, --name NAME
        Filter by a case-insensitive process-name substring.

    -p, --pid PID
        Filter by an exact process ID.

    -t, --top COUNT
        Show at most COUNT results. Zero means no limit.

    --pipe COMMAND
        Send formatted output to COMMAND; a nonzero exit is reported.

    -j, --json, --csv, --tsv, --table
        Select a structured or table output format.

    -h, --help, /?, -?
        Display this help.

    -V, --version
        Display version information.

EXAMPLES
    handles
        Display processes sorted by handle count.

    handles -t 10
        Display the ten processes with the most handles.

    handles -n svchost -s name --asc
        List matching processes in ascending name order.

    handles -p 1234 --json
        Report one process as JSON.

    handles --format tsv > handles.tsv
        Write tab-separated output to a file.

CrossShell for UNIX                                                        handles(1)
)";
}

void HandlesCli::printVersion() {
    std::wcout << L"handles (CrossShell) 5.1.0\n"
               << L"Copyright (c) 2026 PC/OpenSystems LLC contributors. All rights reserved.\n";
}
