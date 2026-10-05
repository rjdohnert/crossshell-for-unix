/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cwchar>

#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. OPTIONS & PRIVILEGE MANAGEMENT
// ============================================================================

struct LnOptions {
    bool symbolic = false;        // -s
    bool force = false;           // -f
    bool interactive = false;     // -i
    bool verbose = false;         // -v
    bool no_deref = false;        // -n, --no-dereference
    bool force_dir = false;       // -F
    bool relative = false;        // -r
    bool show_help = false;
    bool show_version = false;
    std::vector<fs::path> files;
};

class PrivilegeManager {
public:
    static bool EnableSymlinkPrivilege() {
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        TOKEN_PRIVILEGES tp;
        LUID luid;
        if (!LookupPrivilegeValueW(NULL, L"SeCreateSymbolicLinkPrivilege", &luid)) {
            CloseHandle(hToken);
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL res = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        CloseHandle(hToken);
        return res && (GetLastError() == ERROR_SUCCESS);
    }
};

// ============================================================================
// 2. PATH INSPECTOR & CONSOLE PROMPTER
// ============================================================================

class PathInspector {
public:
    static bool PathExists(const fs::path& p) {
        std::error_code ec;
        return fs::exists(p, ec) || fs::is_symlink(p, ec);
    }

    static bool IsDirectoryPath(const fs::path& p, bool no_deref) {
        std::error_code ec;
        if (no_deref && fs::is_symlink(p, ec)) {
            return false;
        }
        return fs::is_directory(p, ec);
    }
};

class ConsolePrompter {
public:
    static bool ConfirmOverwrite(const fs::path& target) {
        std::wcout << L"ln: replace '" << target.wstring() << L"'? (y/N) ";
        std::wstring resp;
        if (std::getline(std::wcin, resp)) {
            if (!resp.empty() && (resp[0] == L'y' || resp[0] == L'Y')) {
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// 3. LINK ENGINE
// ============================================================================

class LinkEngine {
private:
    LnOptions m_opts;

public:
    explicit LinkEngine(LnOptions opts) : m_opts(std::move(opts)) {}

    bool CreateOneLink(const fs::path& source, const fs::path& target) const {
        std::error_code ec;

        if (PathInspector::PathExists(target)) {
            if (m_opts.interactive) {
                if (!ConsolePrompter::ConfirmOverwrite(target)) {
                    return true;
                }
            } else if (!m_opts.force && !m_opts.force_dir) {
                std::fwprintf(stderr, L"ln: '%s': File exists\n", target.c_str());
                return false;
            }

            fs::remove_all(target, ec);
            if (PathInspector::PathExists(target)) {
                std::fwprintf(stderr, L"ln: cannot remove '%s': Permission denied or file in use\n", target.c_str());
                return false;
            }
        }

        if (m_opts.symbolic) {
            fs::path link_target = source;

            if (m_opts.relative) {
                fs::path target_dir = target.parent_path();
                if (target_dir.empty()) target_dir = L".";
                fs::path rel_p = fs::relative(source, target_dir, ec);
                if (!ec && !rel_p.empty()) {
                    link_target = rel_p;
                }
            }

            bool source_is_dir = fs::is_directory(source, ec);

            DWORD flags = 0;
            if (source_is_dir) {
                flags |= 0x1; // SYMBOLIC_LINK_FLAG_DIRECTORY
            }
            flags |= 0x2; // SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE

            if (CreateSymbolicLinkW(target.c_str(), link_target.c_str(), flags)) {
                if (m_opts.verbose) {
                    std::wcout << L"'" << target.wstring() << L"' -> '" << link_target.wstring() << L"'\n";
                }
                return true;
            } else {
                DWORD err = GetLastError();
                if (err == ERROR_PRIVILEGE_NOT_HELD) {
                    std::fwprintf(stderr, L"ln: failed to create symbolic link '%s': Permission denied.\n"
                                          L"    Note: Run CMD/PowerShell as Administrator or enable Windows Developer Mode.\n", target.c_str());
                } else {
                    std::fwprintf(stderr, L"ln: failed to create symbolic link '%s': Win32 error %lu\n", target.c_str(), err);
                }
                return false;
            }
        } else {
            if (fs::is_directory(source, ec)) {
                std::fwprintf(stderr, L"ln: '%s': Is a directory (Hard links to directories are not supported on Windows)\n", source.c_str());
                return false;
            }

            if (CreateHardLinkW(target.c_str(), source.c_str(), NULL)) {
                if (m_opts.verbose) {
                    std::wcout << L"'" << target.wstring() << L"' => '" << source.wstring() << L"'\n";
                }
                return true;
            } else {
                DWORD err = GetLastError();
                std::fwprintf(stderr, L"ln: failed to create hard link '%s' => '%s': Win32 error %lu\n", target.c_str(), source.c_str(), err);
                return false;
            }
        }
    }
};

// ============================================================================
// 4. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    static void PrintUsage() {
        std::wcout << LR"(ln(1)               CrossShell for UNIX Reference Manual                 ln(1)

    NAME
        ln - make links between files

    SYNOPSIS
        ln [OPTIONS] TARGET LINK_NAME
        ln [OPTIONS] TARGET... DIRECTORY

    DESCRIPTION
        ln creates hard links by default, or symbolic links when -s is specified.
        It integrates with NTFS hard links and Windows symbolic links, automatically
        requesting SeCreateSymbolicLinkPrivilege when needed.

    OPTIONS
        -s, --symbolic
            Make symbolic links instead of hard links.

        -f, --force
            Remove existing destination files unconditionally.

        -i, --interactive
            Prompt before removing existing destination files.

        -v, --verbose
            Print the name of each linked file.

        -n, --no-dereference
            Treat destination symbolic link to a directory as a normal file.

        -r, --relative
            Create symbolic links relative to link location.

        -F
            Allow removing existing destination directories.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        ln -s target.txt symlink.txt
            Create a symbolic link named symlink.txt pointing to target.txt.

        ln file1.txt hardlink.txt
            Create an NTFS hard link named hardlink.txt.

        ln -s C:\Source\*.dll C:\App\bin\
            Link multiple files into a target directory.

    CrossShell for UNIX                                                    ln(1)
)";
    }

    static void PrintVersion() {
        std::wcout << L"ln 1.0.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], LnOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"--") {
                for (++i; i < argc; ++i) {
                    opts.files.push_back(argv[i]);
                }
                break;
            }

            if (arg == L"--help" || arg == L"-help" || arg == L"/?") {
                opts.show_help = true;
                return true;
            } else if (arg == L"--version" || arg == L"-V") {
                opts.show_version = true;
                return true;
            } else if (arg == L"-s" || arg == L"--symbolic") {
                opts.symbolic = true;
            } else if (arg == L"-f" || arg == L"--force") {
                opts.force = true;
            } else if (arg == L"-i" || arg == L"--interactive") {
                opts.interactive = true;
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-n" || arg == L"-h" || arg == L"--no-dereference") {
                opts.no_deref = true;
            } else if (arg == L"-F") {
                opts.force_dir = true;
            } else if (arg == L"-r" || arg == L"--relative") {
                opts.relative = true;
            } else if (arg.rfind(L"-", 0) == 0 && arg.length() > 1 && arg[1] != L'-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    wchar_t c = arg[j];
                    if (c == L's') opts.symbolic = true;
                    else if (c == L'f') opts.force = true;
                    else if (c == L'i') opts.interactive = true;
                    else if (c == L'v') opts.verbose = true;
                    else if (c == L'n' || c == L'h') opts.no_deref = true;
                    else if (c == L'F') opts.force_dir = true;
                    else if (c == L'r') opts.relative = true;
                    else {
                        std::fwprintf(stderr, L"ln: invalid option -- '%c'\n", c);
                        return false;
                    }
                }
            } else {
                opts.files.push_back(arg);
            }
        }

        return true;
    }
};

class LnApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        PrivilegeManager::EnableSymlinkPrivilege();

        LnOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return 1;
        }

        if (opts.show_help) {
            OptionParser::PrintUsage();
            return 0;
        }

        if (opts.show_version) {
            OptionParser::PrintVersion();
            return 0;
        }

        if (opts.files.empty()) {
            std::fwprintf(stderr, L"ln: missing file operand\nTry 'ln --help' for more information.\n");
            return 1;
        }

        if (opts.files.size() == 1) {
            std::fwprintf(stderr, L"ln: missing destination file operand after '%ls'\n", opts.files[0].c_str());
            return 1;
        }

        LinkEngine engine(opts);
        bool success = true;

        if (opts.files.size() == 2) {
            fs::path source = opts.files[0];
            fs::path target = opts.files[1];

            if (PathInspector::IsDirectoryPath(target, opts.no_deref)) {
                target = target / source.filename();
            }

            if (!engine.CreateOneLink(source, target)) {
                success = false;
            }
        } else {
            fs::path target_dir = opts.files.back();

            if (!PathInspector::IsDirectoryPath(target_dir, opts.no_deref)) {
                std::fwprintf(stderr, L"ln: target '%ls' is not a directory\n", target_dir.c_str());
                return 1;
            }

            for (size_t i = 0; i < opts.files.size() - 1; ++i) {
                fs::path source = opts.files[i];
                fs::path target = target_dir / source.filename();

                if (!engine.CreateOneLink(source, target)) {
                    success = false;
                }
            }
        }

        return success ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    LnApplication app;
    return app.Run(argc, argv);
}
