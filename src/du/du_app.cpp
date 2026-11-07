/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "du_app.hpp"

#include "du_arg_parser.hpp"
#include "du_help_system.hpp"
#include "du_input_stream_reader.hpp"
#include "du_reporter.hpp"
#include "du_scanner.hpp"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <sys/wait.h>
#endif

namespace du {
namespace {

class FilePipeBuffer final : public std::streambuf {
public:
    explicit FilePipeBuffer(FILE* file) : file_(file) {}

protected:
    int_type overflow(int_type character) override {
        if (traits_type::eq_int_type(character, traits_type::eof())) {
            return sync() == 0 ? traits_type::not_eof(character)
                               : traits_type::eof();
        }
        const char value = traits_type::to_char_type(character);
        return std::fwrite(&value, 1, 1, file_) == 1
            ? character : traits_type::eof();
    }

    std::streamsize xsputn(const char* data, std::streamsize size) override {
        return static_cast<std::streamsize>(
            std::fwrite(data, 1, static_cast<std::size_t>(size), file_));
    }

    int sync() override {
        return std::fflush(file_) == 0 ? 0 : -1;
    }

private:
    FILE* file_;
};

class CommandPipe {
public:
    ~CommandPipe() {
        if (file_) {
            std::string ignored;
            Close(ignored);
        }
    }

    bool Open(const std::string& command, std::string& error) {
#ifdef _WIN32
        file_ = _popen(command.c_str(), "w");
#else
        file_ = popen(command.c_str(), "w");
#endif
        if (!file_) {
            error = "cannot start --pipe command: " +
                    std::error_code(errno, std::generic_category()).message();
            return false;
        }
        buffer_ = std::make_unique<FilePipeBuffer>(file_);
        stream_ = std::make_unique<std::ostream>(buffer_.get());
        return true;
    }

    std::ostream& Stream() { return *stream_; }

    bool Close(std::string& error) {
        if (!file_) return true;
        stream_->flush();
        const bool outputGood = static_cast<bool>(*stream_);
        FILE* file = file_;
        file_ = nullptr;
#ifdef _WIN32
        const int status = _pclose(file);
        const bool commandSucceeded = status == 0;
#else
        const int status = pclose(file);
        const bool commandSucceeded =
            status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
        if (!outputGood) {
            error = "failed while writing output to --pipe command.";
            return false;
        }
        if (!commandSucceeded) {
            error = "--pipe command failed.";
            return false;
        }
        return true;
    }

private:
    FILE* file_ = nullptr;
    std::unique_ptr<FilePipeBuffer> buffer_;
    std::unique_ptr<std::ostream> stream_;
};

bool SetStandardInputBinary(std::string& error) {
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        error = "cannot switch standard input to binary mode.";
        return false;
    }
#else
    (void)error;
#endif
    return true;
}

bool ReadInputPaths(std::istream& input, char delimiter,
                    std::vector<std::filesystem::path>& paths,
                    std::string& error) {
    if (!SetStandardInputBinary(error)) return false;
    return ReadPaths(input, delimiter, paths, error);
}

bool ResolveInputPaths(const Options& options,
                       std::vector<std::filesystem::path>& paths,
                       std::string& error) {
    if (!options.files0From.empty()) {
        if (options.files0From == "-") {
            return ReadInputPaths(std::cin, '\0', paths, error);
        }
        std::ifstream input(options.files0From, std::ios::binary);
        if (!input) {
            error = "cannot open '" + options.files0From +
                    "' for reading.";
            return false;
        }
        return ReadPaths(input, '\0', paths, error);
    }

    if (options.paths.empty()) {
        paths.emplace_back(".");
        return true;
    }
    for (const std::filesystem::path& path : options.paths) {
        if (path == "-") {
            if (!ReadInputPaths(std::cin, '\n', paths, error)) return false;
        } else {
            paths.push_back(path);
        }
    }
    return true;
}

} // namespace

int DuApp::Run(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    ParseResult parsed = ParseArguments(argc, argv);
    if (!parsed.success) {
        std::cerr << "du: " << parsed.error << "\nTry 'du --help' for more "
                  << "information.\n";
        return 2;
    }
    if (parsed.options.showHelp) {
        PrintHelp();
        return 0;
    }
    if (parsed.options.showVersion) {
        PrintVersion();
        return 0;
    }

    std::vector<std::filesystem::path> paths;
    std::string inputError;
    if (!ResolveInputPaths(parsed.options, paths, inputError)) {
        std::cerr << "du: " << inputError << '\n';
        return 1;
    }

    CommandPipe pipe;
    std::ostream* output = &std::cout;
    if (!parsed.options.pipeCommand.empty()) {
        std::string pipeError;
        if (!pipe.Open(parsed.options.pipeCommand, pipeError)) {
            std::cerr << "du: " << pipeError << '\n';
            return 1;
        }
        output = &pipe.Stream();
    }

    DuReporter reporter(parsed.options, *output);
    reporter.Begin();
    DiskUsageScanner scanner(parsed.options);
    const ScanSummary summary = scanner.Scan(
        paths, [&reporter](const UsageEntry& entry) {
            reporter.WriteEntry(entry);
        });
    reporter.Finish(summary);
    output->flush();

    bool outputSucceeded = static_cast<bool>(*output);
    std::string pipeError;
    if (!parsed.options.pipeCommand.empty()) {
        outputSucceeded = pipe.Close(pipeError) && outputSucceeded;
        if (!pipeError.empty()) std::cerr << "du: " << pipeError << '\n';
    } else if (!outputSucceeded) {
        std::cerr << "du: failed while writing output.\n";
    }

    return outputSucceeded && summary.errorCount == 0 ? 0 : 1;
}

} // namespace du
