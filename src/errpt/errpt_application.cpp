#include "errpt_application.hpp"

#include "errpt_event_reader.hpp"
#include "errpt_output.hpp"
#include "errpt_parser.hpp"

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
#include <utility>

namespace {

class CommandPipe {
public:
    CommandPipe() = default;
    CommandPipe(const CommandPipe&) = delete;
    CommandPipe& operator=(const CommandPipe&) = delete;

    ~CommandPipe() {
        if (file_) _pclose(file_);
    }

    bool Open(const std::wstring& command, std::string& error) {
        file_ = _wpopen(command.c_str(), L"wb");
        if (file_) return true;
        error = "cannot open --pipe command (errno " +
            std::to_string(errno) + ").";
        return false;
    }

    bool Write(std::string_view text, std::string& error) {
        if (text.empty()) return true;
        if (std::fwrite(text.data(), 1, text.size(), file_) == text.size()) {
            return true;
        }
        error = "failed while writing to --pipe command.";
        return false;
    }

    bool Close(std::string& error) {
        if (!file_) return true;
        FILE* file = file_;
        file_ = nullptr;
        const bool flushed = std::fflush(file) == 0;
        const int status = _pclose(file);
        if (!flushed) {
            error = "failed to flush output to --pipe command.";
            return false;
        }
        if (status == -1) {
            error = "cannot read the --pipe command exit status.";
            return false;
        }
        if (status != 0) {
            error = "--pipe command exited with status " +
                std::to_string(status) + ".";
            return false;
        }
        return true;
    }

private:
    FILE* file_ = nullptr;
};

} // namespace

int ErrptApplication::Run(int argc, wchar_t* argv[]) const {
    ParseResult parsed = ParseArguments(argc, argv);
    if (!parsed.success) {
        std::wcerr << L"errpt: " << parsed.error
                   << L"\nTry 'errpt --help' for usage.\n";
        return 2;
    }
    if (parsed.options.showHelp) {
        PrintHelp();
        std::cout.flush();
        return std::cout ? 0 : 1;
    }
    if (parsed.options.showVersion) {
        PrintVersion();
        std::cout.flush();
        return std::cout ? 0 : 1;
    }

    EventLogReader reader;
    std::wstring logError;
    if (!reader.Open(parsed.options.logName, logError)) {
        std::wcerr << L"errpt: " << logError << L'\n';
        return 1;
    }

    CommandPipe pipe;
    if (!parsed.options.pipeCommand.empty()) {
        std::string pipeError;
        if (!pipe.Open(parsed.options.pipeCommand, pipeError)) {
            std::wcerr << L"errpt: "
                       << std::wstring(pipeError.begin(), pipeError.end())
                       << L'\n';
            std::wstring closeError;
            if (!reader.Close(closeError)) {
                std::wcerr << L"errpt: " << closeError << L'\n';
            }
            return 1;
        }
    }

    OutputSink sink = [&pipe, &parsed](std::string_view text,
                                      std::string& error) {
        if (!parsed.options.pipeCommand.empty()) {
            return pipe.Write(text, error);
        }
        std::cout.write(text.data(),
                        static_cast<std::streamsize>(text.size()));
        if (std::cout) return true;
        error = "failed while writing standard output.";
        return false;
    };

    EventOutput output(parsed.options.outputFormat, std::move(sink));
    std::string outputError;
    if (!output.Begin(outputError)) {
        std::wcerr << L"errpt: "
                   << std::wstring(outputError.begin(), outputError.end())
                   << L'\n';
        std::wstring closeError;
        if (!reader.Close(closeError)) {
            std::wcerr << L"errpt: " << closeError << L'\n';
        }
        std::string pipeError;
        if (!pipe.Close(pipeError)) {
            std::wcerr << L"errpt: "
                       << std::wstring(pipeError.begin(), pipeError.end())
                       << L'\n';
        }
        return 1;
    }

    std::wstring readError;
    const bool readSucceeded = reader.ReadEach(
        parsed.options,
        [&output, &outputError](const EventEntry& entry) {
            const bool success = output.Write(entry, outputError);
            return success;
        },
        readError);

    std::wstring closeError;
    const bool logClosed = reader.Close(closeError);
    bool outputFinished = false;
    if (outputError.empty()) {
        std::string finishError;
        outputFinished = output.Finish(finishError);
        if (!outputFinished) outputError = std::move(finishError);
    }

    bool pipeClosed = true;
    std::string pipeError;
    if (!parsed.options.pipeCommand.empty()) {
        pipeClosed = pipe.Close(pipeError);
    } else {
        std::cout.flush();
        if (!std::cout && outputError.empty()) {
            outputError = "standard output flush failed.";
        }
    }

    if (!readSucceeded) {
        if (!outputError.empty()) {
            std::wcerr << L"errpt: "
                       << std::wstring(outputError.begin(), outputError.end())
                       << L'\n';
        } else {
            std::wcerr << L"errpt: " << readError << L'\n';
        }
    }
    if (!logClosed) std::wcerr << L"errpt: " << closeError << L'\n';
    if (!outputError.empty() && readSucceeded) {
        std::wcerr << L"errpt: "
                   << std::wstring(outputError.begin(), outputError.end())
                   << L'\n';
    }
    if (!pipeClosed) {
        std::wcerr << L"errpt: "
                   << std::wstring(pipeError.begin(), pipeError.end())
                   << L'\n';
    }

    return readSucceeded && logClosed && outputFinished && outputError.empty() &&
                   pipeClosed
        ? 0 : 1;
}
