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
 * SINGLE FILE INDEX: file.cpp
 * ============================================================================
 * WinFile - Object-Oriented File Type & Magic Signature Identifier for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. FileOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... FileReporter class (JSON/CSV/Table/Pipe)
 * 3. [MAGIC SIGNATURE DETECTOR ENGINE] ..... MagicSignatureDetector, FileEngine classes
 * 4. [APPLICATION CONTROLLER] .............. FileApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cwctype>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class FileOptions {
public:
    bool brief{false};
    bool mime{false};
    std::vector<std::wstring> files;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout << LR"(file(1)             CrossShell for UNIX Reference Manual                 file(1)

    NAME
        file - determine file type

    SYNOPSIS
        file [OPTIONS] FILE...

    DESCRIPTION
        file inspects file system properties, reparse points, headers, and magic
        signatures to determine the classification and type of the specified files.
        It supports detection of Windows PE executables, ELF binaries, archive formats,
        images, documents, and plain or encoded text files.

    OPTIONS
        -b, --brief
            Do not prepend filenames to the classification output.

        -i, --mime
            Output MIME type strings instead of human-readable text.

        --json
            Output file classification metadata in JSON format.

        --csv
            Output file classification metadata in CSV format.

        --table
            Output file classification metadata in tabular format.

        --pipe COMMAND
            Send classification output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        file application.exe
            Determine the type of application.exe.

        file --brief --mime document.pdf
            Display only the MIME type for document.pdf.

        file --json *.dll
            Classify all DLLs in the directory and output structured JSON.

    CrossShell for UNIX                                                    file(1)
)";
    }

    static void printVersion() {
        std::wcout << L"file 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], FileOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i];
            if (a == L"--help" || a == L"-help" || a == L"-h" || a == L"/?") {
                printHelp();
                std::exit(0);
            } else if (a == L"--version" || a == L"-V") {
                printVersion();
                std::exit(0);
            } else if (a == L"-b" || a == L"--brief") {
                opts.brief = true;
            } else if (a == L"-i" || a == L"--mime") {
                opts.mime = true;
            } else if (a == L"--json") {
                opts.outputFormat = 1;
            } else if (a == L"--csv") {
                opts.outputFormat = 2;
            } else if (a == L"--table") {
                opts.outputFormat = 3;
            } else if (a == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (a[0] == L'-' && a.size() > 1) {
                std::wcerr << L"file: unknown option " << a << L"\n";
                return false;
            } else {
                opts.files.push_back(a);
            }
        }

        if (opts.files.empty()) {
            std::wcerr << L"file: missing argument\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class FileReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::vector<std::pair<std::wstring, std::wstring>>& results,
                        int format, bool brief, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == 1) {
            text = L"{\"files\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"path\":\"" + results[i].first + L"\",\"type\":\"" + results[i].second + L"\"}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"path,type\n";
            for (const auto& r : results) {
                text += L"\"" + r.first + L"\",\"" + r.second + L"\"\n";
            }
        } else if (format == 3) {
            text = L"PATH\tTYPE\n--------------------\n";
            for (const auto& r : results) {
                text += r.first + L"\t" + r.second + L"\n";
            }
        } else {
            for (const auto& r : results) {
                if (brief) text += r.second + L"\n";
                else text += r.first + L": " + r.second + L"\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (!pipe) return 1;
            std::string utf8 = toUtf8(text);
            std::fwrite(utf8.data(), 1, utf8.size(), pipe);
            _pclose(pipe);
        } else {
            std::wcout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. MAGIC SIGNATURE DETECTOR ENGINE
// ============================================================================

class MagicSignatureDetector {
private:
    static bool startsWith(const std::vector<unsigned char>& data, const std::vector<unsigned char>& sig) {
        if (data.size() < sig.size()) return false;
        for (size_t i = 0; i < sig.size(); ++i) {
            if (data[i] != sig[i]) return false;
        }
        return true;
    }

    static bool isTextContent(const std::vector<unsigned char>& data) {
        if (data.empty()) return true;
        size_t printable = 0;
        for (unsigned char c : data) {
            if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c <= 126)) {
                ++printable;
            }
        }
        return printable * 100 / data.size() > 90;
    }

public:
    static std::wstring detectType(const std::wstring& path, bool mime) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return mime ? L"cannot-open/not-found" : L"cannot open (No such file or directory)";
        }
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            return mime ? L"inode/directory" : L"directory";
        }
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
            return mime ? L"inode/symlink" : L"symbolic link or reparse point";
        }

        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return mime ? L"cannot-open/permission-denied" : L"cannot open";
        }

        std::vector<unsigned char> data(4096);
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        data.resize(static_cast<size_t>(in.gcount()));

        if (data.empty()) return mime ? L"application/x-empty" : L"empty";

        if (startsWith(data, {0x4D, 0x5A})) return mime ? L"application/x-dosexec" : L"PE executable (MZ)";
        if (startsWith(data, {0x7F, 0x45, 0x4C, 0x46})) return mime ? L"application/x-executable" : L"ELF executable";
        if (startsWith(data, {0x50, 0x4B, 0x03, 0x04})) return mime ? L"application/zip" : L"ZIP archive";
        if (startsWith(data, {0x1F, 0x8B})) return mime ? L"application/gzip" : L"gzip compressed data";
        if (startsWith(data, {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00})) return mime ? L"application/x-xz" : L"XZ compressed data";
        if (startsWith(data, {0x89, 0x50, 0x4E, 0x47})) return mime ? L"image/png" : L"PNG image";
        if (startsWith(data, {0xFF, 0xD8, 0xFF})) return mime ? L"image/jpeg" : L"JPEG image";
        if (startsWith(data, {0x47, 0x49, 0x46, 0x38})) return mime ? L"image/gif" : L"GIF image";
        if (startsWith(data, {0x25, 0x50, 0x44, 0x46})) return mime ? L"application/pdf" : L"PDF document";
        if (startsWith(data, {0x42, 0x4D})) return mime ? L"image/bmp" : L"BMP image";

        if (isTextContent(data)) {
            return mime ? L"text/plain; charset=us-ascii" : L"ASCII text";
        }

        return mime ? L"application/octet-stream" : L"data";
    }
};

class FileEngine {
private:
    FileOptions options;

public:
    explicit FileEngine(FileOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::pair<std::wstring, std::wstring>> results;

        for (const auto& path : options.files) {
            std::wstring type = MagicSignatureDetector::detectType(path, options.mime);
            results.emplace_back(path, type);
        }

        return FileReporter::dispatch(results, options.outputFormat, options.brief, options.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class FileApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        FileOptions options;
        if (!FileOptions::parse(argc, argv, options)) {
            return 1;
        }
        FileEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return FileApp::run(argc, argv);
}
