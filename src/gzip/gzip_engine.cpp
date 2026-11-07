#define WIN32_LEAN_AND_MEAN
#include "gzip_engine.hpp"

#include "gzip_codec.hpp"
#include "gzip_file.hpp"
#include "gzip_report.hpp"

#include <fcntl.h>
#include <io.h>

#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool ReadStandardInput(GzipTempFile& temporary, std::wstring& error) {
    std::array<char, 64 * 1024> buffer{};
    for (;;) {
        std::cin.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = std::cin.gcount();
        if (count > 0 &&
            !temporary.write(buffer.data(), static_cast<size_t>(count), error)) {
            return false;
        }
        if (std::cin.bad()) {
            error = L"failed while reading standard input";
            return false;
        }
        if (std::cin.eof()) break;
        if (std::cin.fail() || count == 0) {
            error = L"standard input stopped before end-of-file";
            return false;
        }
    }
    return true;
}

bool IsGzipSuffix(const std::wstring& filename) {
    if (filename.size() < 3) return false;
    const size_t offset = filename.size() - 3;
    return filename[offset] == L'.' &&
        (filename[offset + 1] == L'g' || filename[offset + 1] == L'G') &&
        (filename[offset + 2] == L'z' || filename[offset + 2] == L'Z');
}

bool ProcessOne(const std::filesystem::path& inputPath, bool stdinInput,
                const GzipOptions& options, bool stdoutMode,
                std::vector<GzipOperation>& operations) {
    std::wstring error;
    GzipTempFile stdinTemporary;
    std::filesystem::path sourcePath = inputPath;

    if (stdinInput) {
        if (!stdinTemporary.createInSystemTemp(error) ||
            !ReadStandardInput(stdinTemporary, error) ||
            !stdinTemporary.finish(error)) {
            std::wcerr << L"gzip: standard input: " << error << L'\n';
            return false;
        }
        sourcePath = stdinTemporary.path();
    } else {
        bool sourceExists = false;
        DWORD sourceAttributes = INVALID_FILE_ATTRIBUTES;
        if (!InspectGzipPath(sourcePath, sourceExists, sourceAttributes, error)) {
            std::wcerr << L"gzip: " << sourcePath.native() << L": " << error << L'\n';
            return false;
        }
        if (!sourceExists) {
            std::wcerr << L"gzip: " << sourcePath.native()
                       << L": input file does not exist\n";
            return false;
        }
        if ((sourceAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) != 0) {
            std::wcerr << L"gzip: " << sourcePath.native()
                       << L": input is not a regular file\n";
            return false;
        }
    }

    std::filesystem::path targetPath;
    if (options.decompress) {
        const std::wstring filename = inputPath.filename().native();
        if (IsGzipSuffix(filename)) {
            targetPath = inputPath;
            targetPath.replace_filename(filename.substr(0, filename.size() - 3));
        } else {
            targetPath = inputPath;
            targetPath += L".out";
        }
    } else {
        targetPath = inputPath;
        targetPath += L".gz";
    }

    if (!stdoutMode) {
        bool targetExists = false;
        DWORD targetAttributes = INVALID_FILE_ATTRIBUTES;
        if (!InspectGzipPath(targetPath, targetExists, targetAttributes, error)) {
            std::wcerr << L"gzip: " << targetPath.native() << L": " << error << L'\n';
            return false;
        }
        if (targetExists &&
            (targetAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE |
                                 FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
            std::wcerr << L"gzip: refusing to replace a directory, device, or "
                          "reparse point: "
                       << targetPath.native() << L'\n';
            return false;
        }
        if (targetExists && !options.force) {
            std::wcerr << L"gzip: " << targetPath.native()
                       << L" already exists; use -f to replace it\n";
            return false;
        }
    }

    GzipTempFile outputTemporary;
    if (!(stdoutMode ? outputTemporary.createInSystemTemp(error)
                     : outputTemporary.createFor(targetPath, error))) {
        std::wcerr << L"gzip: cannot create output for " << inputPath.native()
                   << L": " << error << L'\n';
        return false;
    }

    if (!GzipCodec::run(sourcePath, outputTemporary.path(),
                        !options.decompress, error)) {
        std::wcerr << L"gzip: " << (options.decompress ? L"decompression" : L"compression")
                   << L" failed for " << inputPath.native() << L": " << error << L'\n';
        return false;
    }

    if (stdoutMode) {
        if (!outputTemporary.copyToStdout(error)) {
            std::wcerr << L"gzip: " << error << L'\n';
            return false;
        }
    } else if (!outputTemporary.commit(targetPath, options.force, error)) {
        std::wcerr << L"gzip: " << error << L'\n';
        return false;
    }

    operations.push_back({stdinInput ? std::filesystem::path(L"-") : inputPath,
                          stdoutMode ? std::filesystem::path(L"stdout") : targetPath});

    if (!stdinInput && !options.keep && !stdoutMode) {
        if (!DeleteGzipInput(inputPath, error)) {
            std::wcerr << L"gzip: output created, but " << error << L'\n';
            return false;
        }
    }
    return true;
}
}

int GzipEngine::execute(const GzipOptions& options) {
    std::vector<std::filesystem::path> files = options.files;
    if (files.empty()) files.emplace_back(L"-");

    size_t standardInputCount = 0;
    bool stdoutMode = options.stdoutMode;
    for (const auto& file : files) {
        if (file.native() == L"-") {
            ++standardInputCount;
            stdoutMode = true;
        }
    }
    if (standardInputCount > 1) {
        std::wcerr << L"gzip: standard input may be specified only once\n";
        return 1;
    }
    if (stdoutMode && _setmode(_fileno(stdout), _O_BINARY) == -1) {
        std::wcerr << L"gzip: cannot enable binary standard output\n";
        return 1;
    }
    if (standardInputCount != 0 &&
        _setmode(_fileno(stdin), _O_BINARY) == -1) {
        std::wcerr << L"gzip: cannot enable binary standard input\n";
        return 1;
    }

    std::vector<GzipOperation> operations;
    bool allSucceeded = true;
    for (const auto& file : files) {
        const bool stdinInput = file.native() == L"-";
        if (!ProcessOne(file, stdinInput, options, stdoutMode, operations)) {
            allSucceeded = false;
        }
    }

    std::wstring reportError;
    if (!GzipReport::dispatch(operations, options.reportFormat,
                              options.pipeCommand, stdoutMode, reportError)) {
        std::wcerr << L"gzip: " << reportError << L'\n';
        allSucceeded = false;
    }
    return allSucceeded ? 0 : 1;
}
