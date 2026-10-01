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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <io.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

namespace GetfaclUtility {

    // =========================================================================
    // CLI Options Model
    // =========================================================================
    struct Options {
        bool omit_header = false;          // -c, --omit-header
        bool show_default = true;          // -d, --default (inherited directory ACLs)
        bool show_access = true;           // -a, --access (base and explicit DACLs)
        bool numeric_ids = false;          // -n, --numeric (print raw SIDs instead of names)
        bool skip_base_only = false;       // -s, --skip-base (skip files with only standard owner/group rights)
        bool recursive = false;            // -R, --recursive
        bool follow_symlinks = false;      // -L, --logical vs -P, --physical
        bool tabular = false;              // -t, --tabular
        bool read_stdin = false;           // - (read file list from stdin)
        bool show_help = false;
        bool show_version = false;
        std::vector<std::string> paths;
    };

    // =========================================================================
    // String & Security Descriptor RAII Helpers
    // =========================================================================
    class Win32Helper {
    public:
        static std::string wide_to_utf8(std::wstring_view wstr) {
            if (wstr.empty()) return {};
            int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
            std::string result(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
            return result;
        }

        static std::wstring utf8_to_wide(std::string_view str) {
            if (str.empty()) return {};
            int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
            std::wstring result(size, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size);
            return result;
        }

        static std::string sid_to_string(PSID pSid) {
            if (!pSid || !IsValidSid(pSid)) return "UNKNOWN_SID";
            LPWSTR str_sid = nullptr;
            if (ConvertSidToStringSidW(pSid, &str_sid) && str_sid) {
                std::string result = wide_to_utf8(str_sid);
                LocalFree(str_sid);
                return result;
            }
            return "INVALID_SID";
        }

        static std::string resolve_sid_to_name(PSID pSid, bool numeric, SID_NAME_USE* out_type = nullptr) {
            if (!pSid || !IsValidSid(pSid)) return "UNKNOWN";
            if (numeric) return sid_to_string(pSid);

            wchar_t name[256];
            DWORD name_len = 256;
            wchar_t domain[256];
            DWORD domain_len = 256;
            SID_NAME_USE use;

            if (LookupAccountSidW(nullptr, pSid, name, &name_len, domain, &domain_len, &use)) {
                if (out_type) *out_type = use;
                std::string resolved_name = wide_to_utf8(name);
                std::string resolved_domain = wide_to_utf8(domain);
                if (!resolved_domain.empty()) {
                    return resolved_domain + "\\" + resolved_name;
                }
                return resolved_name;
            }

            return sid_to_string(pSid);
        }

        static std::string mask_to_rwx(DWORD mask) {
            std::string perms = "---";
            // Check Read
            if ((mask & FILE_GENERIC_READ) == FILE_GENERIC_READ || (mask & FILE_READ_DATA) || (mask & GENERIC_READ) || (mask & GENERIC_ALL)) {
                perms[0] = 'r';
            }
            // Check Write
            if ((mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE || (mask & FILE_WRITE_DATA) || (mask & GENERIC_WRITE) || (mask & GENERIC_ALL)) {
                perms[1] = 'w';
            }
            // Check Execute
            if ((mask & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE || (mask & FILE_EXECUTE) || (mask & GENERIC_EXECUTE) || (mask & GENERIC_ALL)) {
                perms[2] = 'x';
            }
            return perms;
        }
    };

    // =========================================================================
    // ACL & ACE Representation Models
    // =========================================================================
    enum class AcePrincipalType {
        User,
        Group,
        Mask,
        Other
    };

    struct ParsedAce {
        bool is_deny = false;
        bool is_inherited = false;
        AcePrincipalType principal_type = AcePrincipalType::User;
        std::string trustee_name;
        std::string trustee_sid;
        DWORD access_mask = 0;
        std::string rwx;
    };

    struct FileAclInfo {
        std::string path;
        std::string owner_name;
        std::string group_name;
        std::string owner_sid;
        std::string group_sid;
        std::vector<ParsedAce> access_entries;
        std::vector<ParsedAce> default_entries;
        bool has_extended_acl = false;
    };

    // =========================================================================
    // Security Inspector Layer (Win32 DACL Extractor)
    // =========================================================================
    class SecurityInspector {
    public:
        static bool inspect_file(const fs::path& file_path, const Options& opts, FileAclInfo& out_info) {
            std::wstring wpath = file_path.wstring();

            PSECURITY_DESCRIPTOR pSD = nullptr;
            PSID pOwner = nullptr;
            PSID pGroup = nullptr;
            PACL pDacl = nullptr;

            DWORD status = GetNamedSecurityInfoW(
                wpath.c_str(),
                SE_FILE_OBJECT,
                OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                &pOwner,
                &pGroup,
                &pDacl,
                nullptr,
                &pSD
            );

            if (status != ERROR_SUCCESS || !pSD) {
                return false;
            }

            // RAII wrapper for LocalFree on Security Descriptor
            std::unique_ptr<void, decltype(&LocalFree)> sd_guard(pSD, LocalFree);

            out_info.path = file_path.string();
            out_info.owner_name = Win32Helper::resolve_sid_to_name(pOwner, opts.numeric_ids);
            out_info.owner_sid = Win32Helper::sid_to_string(pOwner);
            out_info.group_name = Win32Helper::resolve_sid_to_name(pGroup, opts.numeric_ids);
            out_info.group_sid = Win32Helper::sid_to_string(pGroup);

            if (pDacl) {
                ACL_SIZE_INFORMATION acl_info{};
                if (GetAclInformation(pDacl, &acl_info, sizeof(acl_info), AclSizeInformation)) {
                    for (DWORD i = 0; i < acl_info.AceCount; ++i) {
                        LPVOID pAce = nullptr;
                        if (!GetAce(pDacl, i, &pAce)) continue;

                        PACE_HEADER pHeader = reinterpret_cast<PACE_HEADER>(pAce);
                        ParsedAce ace_entry;

                        ace_entry.is_inherited = (pHeader->AceFlags & INHERITED_ACE) != 0;
                        const bool is_container_inherit = (pHeader->AceFlags & (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE)) != 0;

                        PSID sid = nullptr;
                        if (pHeader->AceType == ACCESS_ALLOWED_ACE_TYPE) {
                            auto* allowed = reinterpret_cast<ACCESS_ALLOWED_ACE*>(pAce);
                            ace_entry.is_deny = false;
                            ace_entry.access_mask = allowed->Mask;
                            sid = reinterpret_cast<PSID>(&allowed->SidStart);
                        } else if (pHeader->AceType == ACCESS_DENIED_ACE_TYPE) {
                            auto* denied = reinterpret_cast<ACCESS_DENIED_ACE*>(pAce);
                            ace_entry.is_deny = true;
                            ace_entry.access_mask = denied->Mask;
                            sid = reinterpret_cast<PSID>(&denied->SidStart);
                        } else {
                            continue; // Skip audit/unsupported ACE types
                        }

                        SID_NAME_USE sid_type = SidTypeUnknown;
                        ace_entry.trustee_name = Win32Helper::resolve_sid_to_name(sid, opts.numeric_ids, &sid_type);
                        ace_entry.trustee_sid = Win32Helper::sid_to_string(sid);

                        if (sid_type == SidTypeGroup || sid_type == SidTypeWellKnownGroup || sid_type == SidTypeAlias) {
                            ace_entry.principal_type = AcePrincipalType::Group;
                        } else {
                            ace_entry.principal_type = AcePrincipalType::User;
                        }

                        ace_entry.rwx = Win32Helper::mask_to_rwx(ace_entry.access_mask);

                        if (is_container_inherit) {
                            out_info.default_entries.push_back(ace_entry);
                        }
                        out_info.access_entries.push_back(ace_entry);
                    }
                }
            }

            out_info.has_extended_acl = (out_info.access_entries.size() > 3);
            return true;
        }
    };

    // =========================================================================
    // ACL Output Formatter
    // =========================================================================
    class AclFormatter {
    public:
        static void format(std::ostream& out, const FileAclInfo& info, const Options& opts) {
            if (opts.skip_base_only && !info.has_extended_acl) {
                return;
            }

            if (opts.tabular) {
                format_tabular(out, info, opts);
            } else {
                format_posix(out, info, opts);
            }
        }

    private:
        static void format_posix(std::ostream& out, const FileAclInfo& info, const Options& opts) {
            if (!opts.omit_header) {
                out << "# file: " << info.path << "\n";
                out << "# owner: " << info.owner_name << "\n";
                out << "# group: " << info.group_name << "\n";
            }

            if (opts.show_access) {
                for (const auto& ace : info.access_entries) {
                    if (ace.is_deny) {
                        out << "deny:";
                    }
                    if (ace.principal_type == AcePrincipalType::Group) {
                        out << "group:" << ace.trustee_name << ":" << ace.rwx << "\n";
                    } else {
                        out << "user:" << ace.trustee_name << ":" << ace.rwx << "\n";
                    }
                }
            }

            if (opts.show_default && !info.default_entries.empty()) {
                for (const auto& ace : info.default_entries) {
                    out << "default:";
                    if (ace.is_deny) out << "deny:";
                    if (ace.principal_type == AcePrincipalType::Group) {
                        out << "group:" << ace.trustee_name << ":" << ace.rwx << "\n";
                    } else {
                        out << "user:" << ace.trustee_name << ":" << ace.rwx << "\n";
                    }
                }
            }

            out << "\n";
            out.flush();
        }

        static void format_tabular(std::ostream& out, const FileAclInfo& info, const Options& opts) {
            out << std::left << std::setw(30) << info.path
                << std::setw(25) << info.owner_name
                << std::setw(25) << info.group_name << "\n";

            for (const auto& ace : info.access_entries) {
                out << "  " << (ace.is_deny ? "DENY  " : "ALLOW ")
                    << std::setw(8) << (ace.principal_type == AcePrincipalType::Group ? "group" : "user")
                    << std::setw(30) << ace.trustee_name
                    << std::setw(6) << ace.rwx
                    << (ace.is_inherited ? "[inherited]" : "[explicit]") << "\n";
            }
            out << "\n";
            out.flush();
        }
    };

    // =========================================================================
    // CLI Argument Parser & Manual
    // =========================================================================
    class ArgumentParser {
    public:
        static Options parse(int argc, char* argv[]) {
            Options opts;

            for (int i = 1; i < argc; ++i) {
                std::string_view arg = argv[i];

                if (arg == "-h" || arg == "--help" || arg == "/?") {
                    opts.show_help = true;
                    return opts;
                }
                if (arg == "-v" || arg == "-V" || arg == "--version") {
                    opts.show_version = true;
                    return opts;
                }
                if (arg == "-c" || arg == "--omit-header") {
                    opts.omit_header = true;
                }
                else if (arg == "-a" || arg == "--access") {
                    opts.show_access = true;
                    opts.show_default = false;
                }
                else if (arg == "-d" || arg == "--default") {
                    opts.show_default = true;
                    opts.show_access = false;
                }
                else if (arg == "-n" || arg == "--numeric") {
                    opts.numeric_ids = true;
                }
                else if (arg == "-s" || arg == "--skip-base") {
                    opts.skip_base_only = true;
                }
                else if (arg == "-R" || arg == "--recursive") {
                    opts.recursive = true;
                }
                else if (arg == "-L" || arg == "--logical") {
                    opts.follow_symlinks = true;
                }
                else if (arg == "-P" || arg == "--physical") {
                    opts.follow_symlinks = false;
                }
                else if (arg == "-t" || arg == "--tabular") {
                    opts.tabular = true;
                }
                else if (arg == "-") {
                    opts.read_stdin = true;
                }
                else if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
                    // Bundled flags: e.g. -Rnc
                    for (size_t j = 1; j < arg.size(); ++j) {
                        char flag = arg[j];
                        switch (flag) {
                            case 'c': opts.omit_header = true; break;
                            case 'a': opts.show_access = true; opts.show_default = false; break;
                            case 'd': opts.show_default = true; opts.show_access = false; break;
                            case 'n': opts.numeric_ids = true; break;
                            case 's': opts.skip_base_only = true; break;
                            case 'R': opts.recursive = true; break;
                            case 'L': opts.follow_symlinks = true; break;
                            case 'P': opts.follow_symlinks = false; break;
                            case 't': opts.tabular = true; break;
                            default:
                                throw std::runtime_error(std::string("Unknown flag: -") + flag);
                        }
                    }
                }
                else {
                    opts.paths.emplace_back(arg);
                }
            }

            return opts;
        }

        static void print_help(std::ostream& out) {
            out << R"(getfacl(1)          CrossShell for UNIX Reference Manual                getfacl(1)

    NAME
        getfacl - get file access control lists

    SYNOPSIS
        getfacl [OPTIONS] FILE...

    DESCRIPTION
        getfacl displays the file name, owner, group, and Access Control Lists
        (ACLs) of files and directories on NTFS file systems. It maps Windows
        Security Descriptors, SIDs, and Discretionary Access Control Lists (DACLs)
        into standard POSIX ACL output notation.

    OPTIONS
        -a, --access
            Display the file access control list only.

        -d, --default
            Display the default and inherited directory access control list.

        -c, --omit-header
            Do not display the comment header.

        -n, --numeric
            Print numeric user and group security identifiers (SIDs).

        -s, --skip-base
            Skip files that have only standard/base access entries.

        -R, --recursive
            List the ACLs of all files and directories recursively.

        -L, --logical
            Follow symbolic links during recursion.

        -P, --physical
            Do not follow symbolic links (default).

        -t, --tabular
            Display output in an aligned tabular overview.

        -
            Read the list of target file paths from standard input.

        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

    EXAMPLES
        getfacl file.txt
            Display ACLs for file.txt.

        getfacl -R -c C:\Projects > permissions_audit.txt
            Recursively list ACLs omitting comment headers.

        dir /B | getfacl -
            Read file paths from standard input.

    CrossShell for UNIX                                                   getfacl(1)
)";
        }

        static void print_version(std::ostream& out) {
            out << "getfacl 3.0.0\n"
                << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }
    };

    // =========================================================================
    // Application Lifecycle Controller
    // =========================================================================
    class GetfaclApp {
    public:
        static int run(int argc, char* argv[]) {
            std::ios_base::sync_with_stdio(false);
            std::cin.tie(nullptr);

            try {
                Options opts = ArgumentParser::parse(argc, argv);

                if (opts.show_help) {
                    ArgumentParser::print_help(std::cout);
                    return EXIT_SUCCESS;
                }

                if (opts.show_version) {
                    ArgumentParser::print_version(std::cout);
                    return EXIT_SUCCESS;
                }

                // If stdin is redirected and no paths specified, read from stdin
                if (opts.paths.empty() && !_isatty(_fileno(stdin))) {
                    opts.read_stdin = true;
                }

                if (opts.paths.empty() && !opts.read_stdin) {
                    std::cerr << "getfacl: missing operand\n";
                    std::cerr << "Try 'getfacl --help' for more information.\n";
                    return EXIT_FAILURE;
                }

                int exit_code = EXIT_SUCCESS;

                // Process files from stdin
                if (opts.read_stdin) {
                    std::string line;
                    while (std::getline(std::cin, line)) {
                        if (line.empty()) continue;
                        if (!process_path(line, opts)) {
                            exit_code = EXIT_FAILURE;
                        }
                    }
                }

                // Process explicit CLI paths
                for (const auto& path : opts.paths) {
                    if (path == "-") {
                        std::string line;
                        while (std::getline(std::cin, line)) {
                            if (line.empty()) continue;
                            if (!process_path(line, opts)) {
                                exit_code = EXIT_FAILURE;
                            }
                        }
                    } else {
                        if (!process_path(path, opts)) {
                            exit_code = EXIT_FAILURE;
                        }
                    }
                }

                return exit_code;

            } catch (const std::exception& ex) {
                std::cerr << "getfacl: error: " << ex.what() << "\n";
                return EXIT_FAILURE;
            }
        }

    private:
        static bool process_path(const std::string& path_str, const Options& opts) {
            fs::path p(path_str);
            std::error_code ec;

            if (!fs::exists(p, ec)) {
                std::cerr << "getfacl: " << path_str << ": No such file or directory\n";
                return false;
            }

            bool success = true;

            if (opts.recursive && fs::is_directory(p, ec)) {
                auto dir_opts = opts.follow_symlinks
                    ? fs::directory_options::follow_directory_symlink
                    : fs::directory_options::none;

                FileAclInfo root_info;
                if (SecurityInspector::inspect_file(p, opts, root_info)) {
                    AclFormatter::format(std::cout, root_info, opts);
                } else {
                    std::cerr << "getfacl: " << p.string() << ": Access is denied\n";
                    success = false;
                }

                for (auto it = fs::recursive_directory_iterator(p, dir_opts, ec);
                     it != fs::recursive_directory_iterator(); it.increment(ec)) {
                    if (ec) {
                        std::cerr << "getfacl: " << it->path().string() << ": " << ec.message() << "\n";
                        ec.clear();
                        success = false;
                        continue;
                    }

                    FileAclInfo item_info;
                    if (SecurityInspector::inspect_file(it->path(), opts, item_info)) {
                        AclFormatter::format(std::cout, item_info, opts);
                    } else {
                        std::cerr << "getfacl: " << it->path().string() << ": Access is denied\n";
                        success = false;
                    }
                }
            } else {
                FileAclInfo info;
                if (SecurityInspector::inspect_file(p, opts, info)) {
                    AclFormatter::format(std::cout, info, opts);
                } else {
                    std::cerr << "getfacl: " << path_str << ": Access is denied\n";
                    return false;
                }
            }

            return success;
        }
    };

} // namespace GetfaclUtility

int main(int argc, char* argv[]) {
    return GetfaclUtility::GetfaclApp::run(argc, argv);
}