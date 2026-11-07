#include "env_application.hpp"

#include "env_arguments.hpp"
#include "env_environment.hpp"
#include "env_output.hpp"
#include "env_process.hpp"

#include <windows.h>
#include <cstdio>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

bool WriteUtf8(FILE* output, std::wstring_view text, std::wstring& error) {
    if (text.empty()) return true;
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        error = L"output segment exceeds the supported size.";
        return false;
    }

    const int inputLength = static_cast<int>(text.size());
    const int byteCount = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), inputLength,
        nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0) {
        error = L"cannot encode output as UTF-8 (Windows error " +
            std::to_wstring(GetLastError()) + L").";
        return false;
    }

    std::string bytes(static_cast<std::size_t>(byteCount), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            text.data(), inputLength, bytes.data(),
                            byteCount, nullptr, nullptr) != byteCount) {
        error = L"cannot encode output as UTF-8 (Windows error " +
            std::to_wstring(GetLastError()) + L").";
        return false;
    }
    if (std::fwrite(bytes.data(), 1, bytes.size(), output) != bytes.size()) {
        error = L"failed while writing output.";
        return false;
    }
    return true;
}

class CommandPipe {
public:
    CommandPipe() = default;
    CommandPipe(const CommandPipe&) = delete;
    CommandPipe& operator=(const CommandPipe&) = delete;

    ~CommandPipe() {
        if (file_) _pclose(file_);
    }

    bool Open(const std::wstring& command, std::wstring& error) {
        file_ = _wpopen(command.c_str(), L"wb");
        if (file_) return true;
        error = L"cannot start --pipe command (Windows error " +
            std::to_wstring(GetLastError()) + L").";
        return false;
    }

    bool Write(std::wstring_view text, std::wstring& error) {
        return WriteUtf8(file_, text, error);
    }

    bool Close(std::wstring& error) {
        if (!file_) return true;
        FILE* file = file_;
        file_ = nullptr;
        const bool flushed = std::fflush(file) == 0;
        const int status = _pclose(file);
        if (!flushed) {
            error = L"failed to flush output to --pipe command.";
            return false;
        }
        if (status == -1) {
            error = L"cannot read the --pipe command exit status.";
            return false;
        }
        if (status != 0) {
            error = L"--pipe command exited with status " +
                std::to_wstring(status) + L".";
            return false;
        }
        return true;
    }

private:
    FILE* file_ = nullptr;
};

bool WriteConsole(std::wstring_view text) {
    std::wcout.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(std::wcout);
}

} // namespace

EnvApplication::EnvApplication(ProgramOptions options)
    : options_(std::move(options)) {}

int EnvApplication::Run() {
    if (options_.showHelp) {
        PrintHelp(std::wcout);
        std::wcout.flush();
        return std::wcout ? 0 : 1;
    }
    if (options_.showVersion) {
        PrintVersion(std::wcout);
        std::wcout.flush();
        return std::wcout ? 0 : 1;
    }

    EnvironmentManager environment;
    std::wstring error;
    if (!options_.ignoreEnvironment &&
        !environment.LoadSystemEnvironment(error)) {
        std::wcerr << L"env: " << error << L'\n';
        return 1;
    }

    for (const std::wstring& name : options_.unsetVariables) {
        if (options_.verbose) {
            std::wcerr << L"env: unset " << name << L'\n';
        }
        if (!environment.Unset(name, error)) {
            std::wcerr << L"env: " << error << L'\n';
            return 2;
        }
    }
    for (const auto& [name, value] : options_.setVariables) {
        (void)value;
        if (options_.verbose) {
            std::wcerr << L"env: set " << name << L'\n';
        }
        if (!environment.Set(name, value, error)) {
            std::wcerr << L"env: " << error << L'\n';
            return 2;
        }
    }

    if (options_.commandAndArgs.empty()) {
        if (options_.pipeCommand.empty()) {
            const TextSink sink = [](std::wstring_view text) {
                return WriteConsole(text);
            };
            if (!WriteEnvironment(environment.Values(), options_.outputFormat,
                                  options_.nullDelimiter, sink, error)) {
                std::wcerr << L"env: " << error << L'\n';
                return 1;
            }
            std::wcout.flush();
            if (!std::wcout) {
                std::wcerr << L"env: standard output flush failed.\n";
                return 1;
            }
            return 0;
        }

        CommandPipe pipe;
        if (!pipe.Open(options_.pipeCommand, error)) {
            std::wcerr << L"env: " << error << L'\n';
            return 1;
        }
        const TextSink sink = [&pipe, &error](std::wstring_view text) {
            return pipe.Write(text, error);
        };
        const bool outputGood = WriteEnvironment(
            environment.Values(), options_.outputFormat,
            options_.nullDelimiter, sink, error);
        std::wstring closeError;
        const bool commandGood = pipe.Close(closeError);
        if (!outputGood) {
            std::wcerr << L"env: " << error << L'\n';
            return 1;
        }
        if (!commandGood) {
            std::wcerr << L"env: " << closeError << L'\n';
            return 1;
        }
        return 0;
    }

    std::vector<wchar_t> environmentBlock;
    if (!environment.CreateEnvironmentBlock(environmentBlock, error)) {
        std::wcerr << L"env: " << error << L'\n';
        return 1;
    }
    return ProcessRunner::Execute(
        options_.commandAndArgs, environment.Values(), environmentBlock,
        options_.changeDir, options_.verbose);
}
