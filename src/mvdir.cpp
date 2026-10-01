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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <filesystem>
#include <string>
#include <system_error>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

namespace fs = std::filesystem;

constexpr wchar_t PROG_NAME[] = L"mvdir";
constexpr wchar_t VERSION[]   = L"1.0.0";

// ============================================================================
// 1. STRING & PATH VALIDATION UTILITIES
// ============================================================================
class StringConverter {
public:
    static std::wstring ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), static_cast<int>(str.size()), &wstrTo[0], size_needed);
        return wstrTo;
    }
};

class PathValidator {
public:
    static fs::path NormalizeTrailingSeparator(const fs::path& p) {
        std::wstring path_str = p.wstring();
        while (path_str.length() > 1 && (path_str.back() == L'\\' || path_str.back() == L'/')) {
            if (path_str.length() == 3 && path_str[1] == L':') {
                break;
            }
            path_str.pop_back();
        }
        return fs::path(path_str);
    }

    static bool IsDotOrDotDot(const fs::path& p) {
        fs::path filename = p.filename();
        return (filename == L"." || filename == L"..");
    }

    static bool IsSubpathOrEqual(const fs::path& parent, const fs::path& child) {
        std::error_code ec;
        fs::path canon_parent = fs::weakly_canonical(parent, ec);
        fs::path canon_child = fs::weakly_canonical(child, ec);

        std::wstring parent_str = canon_parent.wstring();
        std::wstring child_str = canon_child.wstring();

        if (!parent_str.empty() && parent_str.back() != L'\\' && parent_str.back() != L'/') {
            parent_str += L'\\';
        }
        if (!child_str.empty() && child_str.back() != L'\\' && child_str.back() != L'/') {
            child_str += L'\\';
        }

        if (child_str.length() < parent_str.length()) {
            return false;
        }

        return _wcsnicmp(parent_str.c_str(), child_str.c_str(), parent_str.length()) == 0;
    }
};

// ============================================================================
// 2. DIRECTORY MOVER ENGINE
// ============================================================================
class DirectoryMover {
public:
    static void PrepareForDeletion(const fs::path& p) {
        std::error_code ec;
        if (fs::is_symlink(p, ec)) return;

        if (fs::is_directory(p, ec)) {
            for (const auto& entry : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
                PrepareForDeletion(entry.path());
            }
        }

        DWORD attrs = GetFileAttributesW(p.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
            SetFileAttributesW(p.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
        }
    }

    static bool CopyAndRemoveDir(const fs::path& src, const fs::path& dest, std::wstring& err_msg) {
        std::error_code ec;

        if (fs::is_symlink(src, ec)) {
            fs::copy_symlink(src, dest, ec);
            if (ec) {
                err_msg = L"Failed to copy directory symlink: " + StringConverter::ToWide(ec.message());
                return false;
            }
            fs::remove(src, ec);
            return true;
        }

        fs::create_directories(dest, ec);
        if (ec) {
            err_msg = L"Failed to create target directory: " + StringConverter::ToWide(ec.message());
            return false;
        }

        for (const auto& entry : fs::recursive_directory_iterator(src, fs::directory_options::skip_permission_denied, ec)) {
            const auto& src_subpath = entry.path();
            auto rel_path = fs::relative(src_subpath, src, ec);
            fs::path dest_subpath = dest / rel_path;

            if (fs::is_symlink(src_subpath, ec)) {
                fs::copy_symlink(src_subpath, dest_subpath, ec);
            } else if (fs::is_directory(src_subpath, ec)) {
                fs::create_directory(dest_subpath, ec);
            } else if (fs::is_regular_file(src_subpath, ec)) {
                fs::copy_file(src_subpath, dest_subpath, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    auto last_time = fs::last_write_time(src_subpath, ec);
                    if (!ec) fs::last_write_time(dest_subpath, last_time, ec);
                }
            }

            if (ec) {
                err_msg = L"Failed copying " + src_subpath.wstring() + L": " + StringConverter::ToWide(ec.message());
                PrepareForDeletion(dest);
                std::error_code clean_ec;
                fs::remove_all(dest, clean_ec);
                return false;
            }
        }

        auto src_time = fs::last_write_time(src, ec);
        if (!ec) fs::last_write_time(dest, src_time, ec);

        PrepareForDeletion(src);
        fs::remove_all(src, ec);
        if (ec) {
            err_msg = L"Copied to destination, but failed to delete original directory: " + StringConverter::ToWide(ec.message());
            return false;
        }

        return true;
    }
};

// ============================================================================
// 3. OPTION PARSER & HELP SYSTEM
// ============================================================================
struct MvdirOptions {
    bool show_help = false;
    bool show_version = false;
    fs::path source;
    fs::path destination;
};

class OptionParser {
public:
    static void PrintVersion() {
        std::wcout << PROG_NAME << L" version " << VERSION << L"\n"
                   << L"Copyright (c) Roberto J Dohnert\n"
                   << L"Licensed under the BSD 3-Clause License.\n";
    }

    static void PrintHelp() {
        std::wcout << LR"(mvdir(1)                CrossShell for UNIX Reference Manual                  mvdir(1)

    NAME
        mvdir - move or rename directories

    SYNOPSIS
        mvdir [OPTIONS] DIRECTORY1 DIRECTORY2

    DESCRIPTION
        mvdir moves or renames Directory1 to Directory2.

        If Directory2 exists and is a directory, Directory1 is moved inside
        Directory2 as a subdirectory (Directory2\Directory1). If Directory2
        does not exist, Directory1 is renamed to Directory2, provided
        Directory2's parent directory exists.

        If Directory1 and Directory2 reside on different drives or volumes,
        a recursive copy-and-delete operation is automatically performed.

    OPTIONS
        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        mvdir oldname newname
            Rename directory oldname to newname.

        mvdir myproject D:\Archive\
            Move directory myproject into D:\Archive\.

        mvdir C:\Logs D:\LogsBackup
            Move directory tree across volumes.

    CrossShell for UNIX                                                    mvdir(1)
)";
    }

    bool Parse(int argc, wchar_t* argv[], MvdirOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                opts.show_help = true;
                return true;
            }
            if (arg == L"--version" || arg == L"-v") {
                opts.show_version = true;
                return true;
            }
        }

        if (argc != 3) {
            std::wcerr << L"Usage: mvdir Directory1 Directory2\n"
                       << L"Try 'mvdir --help' for more information.\n";
            return false;
        }

        opts.source = PathValidator::NormalizeTrailingSeparator(argv[1]);
        opts.destination = PathValidator::NormalizeTrailingSeparator(argv[2]);
        return true;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================
class MvdirApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);

        MvdirOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return (argc > 1 && (std::wstring(argv[1]) == L"-h" || std::wstring(argv[1]) == L"--help" || std::wstring(argv[1]) == L"/?")) ? 0 : 1;
        }

        if (opts.show_help) {
            OptionParser::PrintHelp();
            return 0;
        }

        if (opts.show_version) {
            OptionParser::PrintVersion();
            return 0;
        }

        const auto& src = opts.source;
        const auto& dest_input = opts.destination;

        if (PathValidator::IsDotOrDotDot(src) || PathValidator::IsDotOrDotDot(dest_input)) {
            std::wcerr << PROG_NAME << L": Cannot move or overwrite '.' or '..'\n";
            return 1;
        }

        if (src.has_root_path() && src == src.root_path()) {
            std::wcerr << PROG_NAME << L": Cannot move root directory (" << src.wstring() << L")\n";
            return 1;
        }

        std::error_code ec;
        if (!fs::exists(src, ec)) {
            std::wcerr << PROG_NAME << L": " << src.wstring() << L": No such file or directory\n";
            return 1;
        }

        if (!fs::is_directory(src, ec)) {
            std::wcerr << PROG_NAME << L": " << src.wstring() << L": Not a directory\n";
            return 1;
        }

        fs::path target_path;
        if (fs::exists(dest_input, ec)) {
            if (fs::is_directory(dest_input, ec)) {
                target_path = dest_input / src.filename();
                if (fs::exists(target_path, ec)) {
                    std::wcerr << PROG_NAME << L": " << target_path.wstring() << L": Directory already exists\n";
                    return 1;
                }
            } else {
                std::wcerr << PROG_NAME << L": " << dest_input.wstring() << L": Destination is a file, not a directory\n";
                return 1;
            }
        } else {
            target_path = dest_input;
            fs::path parent = target_path.parent_path();
            if (!parent.empty() && !fs::is_directory(parent, ec)) {
                std::wcerr << PROG_NAME << L": " << parent.wstring() << L": Parent directory does not exist\n";
                return 1;
            }
        }

        if (PathValidator::IsSubpathOrEqual(src, target_path)) {
            std::wcerr << PROG_NAME << L": Cannot move directory " << src.wstring() 
                       << L" into itself or a subdirectory of itself (" << target_path.wstring() << L")\n";
            return 1;
        }

        fs::rename(src, target_path, ec);
        if (!ec) {
            return 0;
        }

        if (ec == std::errc::cross_device_link || ec.value() == ERROR_NOT_SAME_DEVICE) {
            std::wstring copy_err;
            if (DirectoryMover::CopyAndRemoveDir(src, target_path, copy_err)) {
                return 0;
            } else {
                std::wcerr << PROG_NAME << L": " << copy_err << L"\n";
                return 1;
            }
        }

        std::wcerr << PROG_NAME << L": Failed to move " << src.wstring() 
                   << L" to " << target_path.wstring() 
                   << L": " << StringConverter::ToWide(ec.message()) << L"\n";
        return 1;
    }
};

// ============================================================================
// 5. ENTRY POINT
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    MvdirApplication app;
    return app.Run(argc, argv);
}