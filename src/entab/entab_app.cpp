/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "entab_app.hpp"

#include "entab_arg_parser.hpp"
#include "entab_engine.hpp"
#include "entab_help.hpp"
#include "entab_reporter.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
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
#include <unistd.h>
#include <sys/wait.h>
#endif

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
        file_ = _popen(command.c_str(), "wb");
#else
        file_ = popen(command.c_str(), "w");
#endif
        if (!file_) {
            error = "cannot start --pipe command: " +
                std::error_code(errno, std::generic_category()).message();
            return false;
        }
#ifdef _WIN32
        if (_setmode(_fileno(file_), _O_BINARY) == -1) {
            error = "cannot switch --pipe to binary mode.";
            Close(error);
            return false;
        }
#endif
        buffer_ = std::make_unique<FilePipeBuffer>(file_);
        stream_ = std::make_unique<std::ostream>(buffer_.get());
        return true;
    }

    std::ostream& Stream() { return *stream_; }

    bool Close(std::string& error) {
        if (!file_) return true;
        if (stream_) stream_->flush();
        const bool outputGood = !stream_ || static_cast<bool>(*stream_);
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

bool SetBinaryMode(std::string& error) {
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        error = "cannot switch standard input to binary mode.";
        return false;
    }
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        error = "cannot switch standard output to binary mode.";
        return false;
    }
#else
    (void)error;
#endif
    return true;
}

bool IsInteractiveInput() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

std::string FileOpenError() {
    return std::error_code(errno, std::generic_category()).message();
}

} // namespace

int EntabApp::Run(int argc, char* argv[]) {
    if (argc == 1 && IsInteractiveInput()) {
        PrintEntabHelp(argv[0]);
        return 0;
    }

    EntabParseResult parsed = ParseEntabArguments(argc, argv);
    if (!parsed.success) {
        std::cerr << "entab: " << parsed.error << "\nTry 'entab --help' for "
                  << "more information.\n";
        return 2;
    }
    if (parsed.options.showHelp) {
        PrintEntabHelp(argv[0]);
        return 0;
    }
    if (parsed.options.showVersion) {
        PrintEntabVersion();
        return 0;
    }

    std::string modeError;
    if (!SetBinaryMode(modeError)) {
        std::cerr << "entab: " << modeError << '\n';
        return 1;
    }
    std::ios_base::sync_with_stdio(false);

    CommandPipe pipe;
    std::ostream* output = &std::cout;
    if (!parsed.options.pipeCommand.empty()) {
        std::string pipeError;
        if (!pipe.Open(parsed.options.pipeCommand, pipeError)) {
            std::cerr << "entab: " << pipeError << '\n';
            return 1;
        }
        output = &pipe.Stream();
    }

    EntabReporter reporter(parsed.options.outputFormat, *output);
    bool success = reporter.Begin();
    bool outputUsable = success;
    if (!success) {
        std::cerr << "entab: output initialization failed.\n";
    }

    EntabEngine engine(parsed.options);
    const std::vector<EntabInput> files = parsed.options.files.empty()
        ? std::vector<EntabInput>{{"-", true}} : parsed.options.files;
    for (const EntabInput& file : files) {
        if (!outputUsable) break;

        if (file.standardInput) {
            std::string error;
            if (!engine.Convert(std::cin, reporter, error)) {
                std::cerr << "entab: standard input: " << error << '\n';
                success = false;
                outputUsable = false;
            }
            continue;
        }

        errno = 0;
        std::ifstream input(file.path, std::ios::binary);
        if (!input) {
            std::cerr << "entab: " << file.path << ": "
                      << FileOpenError() << '\n';
            success = false;
            continue;
        }
        std::string error;
        if (!engine.Convert(input, reporter, error)) {
            std::cerr << "entab: " << file.path << ": " << error << '\n';
            success = false;
            outputUsable = false;
        }
    }

    if (!reporter.Finish()) {
        std::cerr << "entab: output finalization failed.\n";
        success = false;
    }
    output->flush();
    if (!*output) {
        std::cerr << "entab: output write failed.\n";
        success = false;
    }

    if (!parsed.options.pipeCommand.empty()) {
        std::string pipeError;
        if (!pipe.Close(pipeError)) {
            std::cerr << "entab: " << pipeError << '\n';
            success = false;
        }
    } else {
        std::cout.flush();
        if (!std::cout) {
            std::cerr << "entab: standard output flush failed.\n";
            success = false;
        }
    }

    return success ? 0 : 1;
}
