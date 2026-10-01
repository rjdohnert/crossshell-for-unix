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
 * SINGLE FILE INDEX: dos2unix.cpp
 * ============================================================================
 * WinDos2Unix - Object-Oriented CRLF <-> LF Line Ending Converter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. Dos2UnixOptions class (CLI parsing & flags)
 * 2. [TIMESTAMP & FILE UTILITIES] .......... FileTimestampHelper class (preservation)
 * 3. [STRUCTURED OUTPUT REPORTER] .......... Dos2UnixReporter class (JSON/CSV/Table/Pipe)
 * 4. [LINE CONVERTER ENGINE] ............... LineEndingConverter class (stream/file conversion)
 * 5. [APPLICATION CONTROLLER] .............. Dos2UnixApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cwchar>
#include <cwctype>
#include <memory>

#pragma comment(lib, "Shell32.lib")

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

enum class LineEndingMode {
    DosToUnix,
    UnixToDos
};

class Dos2UnixOptions {
public:
    LineEndingMode mode{LineEndingMode::DosToUnix};
    bool preserveDate{false};
    bool forceBinary{false};
    bool quiet{false};
    bool verbose{false};
    bool newFileMode{false};
    int outputFormat{0};
    std::wstring pipeCommand;
    std::vector<std::pair<std::wstring, std::wstring>> filePairs;

    static void printHelp(LineEndingMode mode) {
        if (mode == LineEndingMode::DosToUnix) {
            std::wcout << LR"(dos2unix(1)                CrossShell for UNIX Reference Manual                dos2unix(1)

    NAME
        dos2unix - DOS/Mac to Unix and vice versa text file format converter

    SYNOPSIS
        dos2unix [OPTIONS] [FILE]...
        dos2unix [OPTIONS] -n INFILE OUTFILE...

    DESCRIPTION
        Converts text files between DOS/Windows line endings and Unix line endings.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -k, --keepdate
            Keep output file date (timestamp preservation).
        -f, --force
            Force conversion of binary files.
        -q, --quiet
            Quiet mode, suppress all warnings.
        -v, --verbose
            Verbose operation output.
        -n, --newfile
            New file mode (INFILE OUTFILE pairs).
        --json, --csv
            Emit structured output.
        --pipe COMMAND
            Send output through COMMAND.
        -h, --help
            Display this reference manual and exit.
        -V, --version
            Display version information and exit.

    EXAMPLES
        dos2unix README.txt
            Convert README.txt in-place from DOS (CRLF) to Unix (LF).

        dos2unix -k -v *.txt
            Convert all matching text files preserving timestamp.

        dos2unix -n input.txt output.txt
            Convert input.txt and write output to output.txt.

    CrossShell for UNIX                                                    dos2unix(1)
)";
        } else {
            std::wcout << LR"(unix2dos(1)                CrossShell for UNIX Reference Manual                unix2dos(1)

    NAME
        unix2dos - Unix to DOS/Mac and vice versa text file format converter

    SYNOPSIS
        unix2dos [OPTIONS] [FILE]...
        unix2dos [OPTIONS] -n INFILE OUTFILE...

    DESCRIPTION
        Converts text files between Unix line endings and DOS/Windows line endings.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -k, --keepdate
            Keep output file date (timestamp preservation).
        -f, --force
            Force conversion of binary files.
        -q, --quiet
            Quiet mode, suppress all warnings.
        -v, --verbose
            Verbose operation output.
        -n, --newfile
            New file mode (INFILE OUTFILE pairs).
        --json, --csv
            Emit structured output.
        --pipe COMMAND
            Send output through COMMAND.
        -h, --help
            Display this reference manual and exit.
        -V, --version
            Display version information and exit.

    EXAMPLES
        unix2dos README.txt
            Convert README.txt in-place from Unix (LF) to DOS (CRLF).

        unix2dos -k -v *.txt
            Convert all matching text files preserving timestamp.

        unix2dos -n input.txt output.txt
            Convert input.txt and write output to output.txt.

    CrossShell for UNIX                                                    unix2dos(1)
)";
        }
    }

    static void printVersion(LineEndingMode mode) {
        if (mode == LineEndingMode::DosToUnix) {
            std::wcout << L"dos2unix 7.5.2\n";
        } else {
            std::wcout << L"unix2dos 7.5.2\n";
        }
    }

    static bool parse(int argc, wchar_t* argv[], Dos2UnixOptions& opts) {
        std::wstring exeName = argv[0] ? argv[0] : L"";
        size_t lastSlash = exeName.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) exeName = exeName.substr(lastSlash + 1);
        for (auto& c : exeName) c = std::towlower(c);

        if (exeName.find(L"unix2dos") != std::wstring::npos) {
            opts.mode = LineEndingMode::UnixToDos;
        } else {
            opts.mode = LineEndingMode::DosToUnix;
        }

        std::vector<std::wstring> positional;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printHelp(opts.mode);
                std::exit(0);
            } else if (arg == L"-V" || arg == L"--version") {
                printVersion(opts.mode);
                std::exit(0);
            } else if (arg == L"-k" || arg == L"--keepdate") {
                opts.preserveDate = true;
            } else if (arg == L"-f" || arg == L"--force") {
                opts.forceBinary = true;
            } else if (arg == L"-q" || arg == L"--quiet") {
                opts.quiet = true;
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-n" || arg == L"--newfile") {
                opts.newFileMode = true;
            } else if (arg == L"--json") {
                opts.outputFormat = 1;
            } else if (arg == L"--csv") {
                opts.outputFormat = 2;
            } else if (arg == L"--table") {
                opts.outputFormat = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else {
                positional.push_back(arg);
            }
        }

        if (opts.newFileMode) {
            if (positional.size() % 2 != 0) {
                std::wcerr << L"dos2unix: new file mode requires pairs of input and output files\n";
                return false;
            }
            for (size_t k = 0; k < positional.size(); k += 2) {
                opts.filePairs.emplace_back(positional[k], positional[k + 1]);
            }
        } else {
            for (const auto& p : positional) {
                opts.filePairs.emplace_back(p, p);
            }
        }

        return true;
    }
};

// ============================================================================
// 2. TIMESTAMP & FILE UTILITIES
// ============================================================================

struct FileTimeInfo {
    FILETIME creationTime{};
    FILETIME lastAccessTime{};
    FILETIME lastWriteTime{};
    bool valid{false};
};

class FileTimestampHelper {
public:
    static FileTimeInfo getTimestamps(const std::wstring& path) {
        FileTimeInfo info;
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            if (GetFileTime(hFile, &info.creationTime, &info.lastAccessTime, &info.lastWriteTime)) {
                info.valid = true;
            }
            CloseHandle(hFile);
        }
        return info;
    }

    static void setTimestamps(const std::wstring& path, const FileTimeInfo& info) {
        if (!info.valid) return;
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            SetFileTime(hFile, &info.creationTime, &info.lastAccessTime, &info.lastWriteTime);
            CloseHandle(hFile);
        }
    }
};

// ============================================================================
// 3. STRUCTURED OUTPUT REPORTER
// ============================================================================

class Dos2UnixReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::vector<std::pair<std::wstring, std::wstring>>& conversions,
                        int format, const std::wstring& pipeCommand) {
        if (format == 0 && pipeCommand.empty()) return 0;

        std::wstring text;
        if (format == 1) {
            text = L"{\"conversions\":[";
            for (size_t i = 0; i < conversions.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"in\":\"" + conversions[i].first + L"\",\"out\":\"" + conversions[i].second + L"\"}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"input,output\n";
            for (const auto& c : conversions) {
                text += L"\"" + c.first + L"\",\"" + c.second + L"\"\n";
            }
        } else if (format == 3) {
            text = L"INPUT\tOUTPUT\n--------------------\n";
            for (const auto& c : conversions) {
                text += c.first + L"\t" + c.second + L"\n";
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
// 4. LINE CONVERTER ENGINE
// ============================================================================

class LineEndingConverter {
private:
    Dos2UnixOptions options;

    static bool isBinary(const std::vector<char>& buffer, size_t bytesRead) {
        for (size_t i = 0; i < bytesRead; ++i) {
            if (buffer[i] == '\0') return true;
        }
        return false;
    }

    bool convertStream(std::istream& in, std::ostream& out) const {
        std::vector<char> inBuf(65536);
        std::vector<char> outBuf(65536);
        size_t outPos = 0;

        auto flushOut = [&]() {
            if (outPos > 0) {
                out.write(outBuf.data(), outPos);
                outPos = 0;
            }
        };

        auto emitByte = [&](char b) {
            outBuf[outPos++] = b;
            if (outPos == outBuf.size()) flushOut();
        };

        if (options.mode == LineEndingMode::DosToUnix) {
            char ch = 0;
            while (in.get(ch)) {
                if (ch == '\r') {
                    int next = in.peek();
                    if (next == '\n') {
                        continue;
                    }
                }
                emitByte(ch);
            }
        } else {
            char ch = 0;
            while (in.get(ch)) {
                if (ch == '\n') {
                    emitByte('\r');
                    emitByte('\n');
                } else if (ch == '\r') {
                    int next = in.peek();
                    if (next == '\n') {
                        emitByte('\r');
                        emitByte('\n');
                        in.get();
                    } else {
                        emitByte('\r');
                    }
                } else {
                    emitByte(ch);
                }
            }
        }

        flushOut();
        return true;
    }

public:
    explicit LineEndingConverter(Dos2UnixOptions opts) : options(std::move(opts)) {}

    int execute() {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        if (options.filePairs.empty()) {
            convertStream(std::cin, std::cout);
            return 0;
        }

        std::vector<std::pair<std::wstring, std::wstring>> converted;
        bool allOk = true;

        for (const auto& pair : options.filePairs) {
            const std::wstring& inFile = pair.first;
            const std::wstring& outFile = pair.second;

            if (inFile == L"-") {
                convertStream(std::cin, std::cout);
                converted.emplace_back(L"STDIN", L"STDOUT");
                continue;
            }

            FileTimeInfo times = options.preserveDate ? FileTimestampHelper::getTimestamps(inFile) : FileTimeInfo{};

            std::ifstream in(inFile, std::ios::binary);
            if (!in.is_open()) {
                if (!options.quiet) std::wcerr << L"dos2unix: cannot open " << inFile << L"\n";
                allOk = false;
                continue;
            }

            std::wstring tempOut = outFile + L".tmp_d2u";
            std::ofstream out(tempOut, std::ios::binary);
            if (!out.is_open()) {
                if (!options.quiet) std::wcerr << L"dos2unix: cannot create " << tempOut << L"\n";
                allOk = false;
                continue;
            }

            convertStream(in, out);
            in.close();
            out.close();

            if (inFile == outFile) {
                DeleteFileW(inFile.c_str());
            }
            MoveFileExW(tempOut.c_str(), outFile.c_str(), MOVEFILE_REPLACE_EXISTING);

            if (options.preserveDate && times.valid) {
                FileTimestampHelper::setTimestamps(outFile, times);
            }

            if (options.verbose && !options.quiet) {
                std::wcout << L"converted " << inFile << L" -> " << outFile << L"\n";
            }
            converted.emplace_back(inFile, outFile);
        }

        Dos2UnixReporter::dispatch(converted, options.outputFormat, options.pipeCommand);
        return allOk ? 0 : 1;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class Dos2UnixApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        Dos2UnixOptions options;
        if (!Dos2UnixOptions::parse(argc, argv, options)) {
            return 1;
        }
        LineEndingConverter engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return Dos2UnixApp::run(argc, argv);
}
