/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "options.hpp"
#include <iostream>
#include <cwctype>
#include <cstdlib>

namespace dos2unix {

void Dos2UnixOptions::printHelp(LineEndingMode mode) {
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

void Dos2UnixOptions::printVersion(LineEndingMode mode) {
    if (mode == LineEndingMode::DosToUnix) {
        std::wcout << L"dos2unix 7.5.2\n";
    } else {
        std::wcout << L"unix2dos 7.5.2\n";
    }
}

bool Dos2UnixOptions::parse(int argc, wchar_t* argv[], Dos2UnixOptions& opts) {
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

} // namespace dos2unix
