#define WIN32_LEAN_AND_MEAN
#include "handles_app.hpp"

#include "handles_collect.hpp"
#include "handles_format.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
bool ContainsName(const std::wstring& name, const std::wstring& needle) {
    if (needle.empty()) return true;
    if (needle.size() > name.size() ||
        needle.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    const int length = static_cast<int>(needle.size());
    for (size_t offset = 0; offset <= name.size() - needle.size(); ++offset) {
        if (CompareStringOrdinal(name.data() + offset, length,
                needle.data(), length, TRUE) == CSTR_EQUAL) {
            return true;
        }
    }
    return false;
}

int CompareNames(const std::wstring& left, const std::wstring& right) {
    const int leftLength = static_cast<int>(left.size());
    const int rightLength = static_cast<int>(right.size());
    const int result = CompareStringOrdinal(left.c_str(), leftLength,
                                            right.c_str(), rightLength, TRUE);
    if (result == CSTR_LESS_THAN) return -1;
    if (result == CSTR_GREATER_THAN) return 1;
    return 0;
}

bool WriteOutput(const std::string& output, std::wstring& error) {
    HANDLE standardOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    if (standardOutput == nullptr || standardOutput == INVALID_HANDLE_VALUE) {
        error = L"cannot access standard output";
        return false;
    }

    DWORD consoleMode = 0;
    if (GetConsoleMode(standardOutput, &consoleMode)) {
        if (output.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
            error = L"output is too large for console conversion";
            return false;
        }
        const int required = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, output.data(),
            static_cast<int>(output.size()), nullptr, 0);
        if (required == 0 && !output.empty()) {
            error = L"cannot decode UTF-8 output for the console";
            return false;
        }
        std::wstring wide(static_cast<size_t>(required), L'\0');
        if (required != 0 && MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, output.data(),
                static_cast<int>(output.size()), wide.data(), required) != required) {
            error = L"cannot decode UTF-8 output for the console";
            return false;
        }

        std::wstring consoleText;
        consoleText.reserve(wide.size() + 16);
        for (wchar_t character : wide) {
            if (character == L'\n' &&
                (consoleText.empty() || consoleText.back() != L'\r')) {
                consoleText.push_back(L'\r');
            }
            consoleText.push_back(character);
        }
        size_t writtenTotal = 0;
        while (writtenTotal < consoleText.size()) {
            const DWORD request = static_cast<DWORD>((std::min)(
                consoleText.size() - writtenTotal,
                static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
            DWORD written = 0;
            if (!WriteConsoleW(standardOutput, consoleText.data() + writtenTotal,
                               request, &written, nullptr) || written == 0) {
                error = L"failed while writing output to the console";
                return false;
            }
            writtenTotal += written;
        }
        return true;
    }

    size_t writtenTotal = 0;
    while (writtenTotal < output.size()) {
        const DWORD request = static_cast<DWORD>((std::min)(
            output.size() - writtenTotal,
            static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written = 0;
        if (!WriteFile(standardOutput, output.data() + writtenTotal,
                       request, &written, nullptr) || written == 0) {
            error = L"failed while writing output";
            return false;
        }
        writtenTotal += written;
    }
    return true;
}

bool SendToPipe(const std::wstring& command, const std::string& output,
                std::wstring& error) {
    FILE* pipe = _wpopen(command.c_str(), L"wb");
    if (pipe == nullptr) {
        error = L"cannot start the --pipe command";
        return false;
    }
    const size_t written = std::fwrite(output.data(), 1, output.size(), pipe);
    const bool flushed = std::fflush(pipe) == 0;
    const int result = _pclose(pipe);
    if (written != output.size() || !flushed || result != 0) {
        error = L"failed to deliver output through --pipe";
        return false;
    }
    return true;
}
}

int HandlesApp::run(const AppConfig& config) {
    std::vector<ProcessHandleInfo> processes;
    std::wstring error;
    if (!HandlesCollector::collect(processes, error)) {
        std::wcerr << L"handles: " << error << L'\n';
        return 1;
    }

    processes.erase(std::remove_if(processes.begin(), processes.end(),
        [&config](const ProcessHandleInfo& process) {
            return (config.filterPid && process.pid != *config.filterPid) ||
                   !ContainsName(process.name, config.filterName);
        }), processes.end());

    std::sort(processes.begin(), processes.end(),
        [&config](const ProcessHandleInfo& left,
                  const ProcessHandleInfo& right) {
            int comparison = 0;
            if (config.sortBy == SortColumn::handles) {
                const bool leftAvailable =
                    left.status == HandleCountStatus::available;
                const bool rightAvailable =
                    right.status == HandleCountStatus::available;
                if (leftAvailable != rightAvailable) return leftAvailable;
                if (leftAvailable && left.handleCount != right.handleCount) {
                    return config.sortDescending
                        ? left.handleCount > right.handleCount
                        : left.handleCount < right.handleCount;
                }
            } else if (config.sortBy == SortColumn::pid) {
                if (left.pid != right.pid) {
                    return config.sortDescending
                        ? left.pid > right.pid
                        : left.pid < right.pid;
                }
            } else {
                comparison = CompareNames(left.name, right.name);
                if (comparison != 0) {
                    return config.sortDescending
                        ? comparison > 0
                        : comparison < 0;
                }
            }
            return left.pid < right.pid;
        });

    if (config.topN != 0 && config.topN < processes.size()) {
        processes.resize(config.topN);
    }

    std::string output;
    if (!FormatProcesses(config.format, processes, output, error)) {
        std::wcerr << L"handles: " << error << L'\n';
        return 1;
    }
    const bool sent = config.pipeCommand.empty()
        ? WriteOutput(output, error)
        : SendToPipe(config.pipeCommand, output, error);
    if (!sent) {
        std::wcerr << L"handles: " << error << L'\n';
        return 1;
    }
    return 0;
}
