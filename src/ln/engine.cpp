#include "engine.hpp"
#include <iostream>
#include <string>
#include <cwchar>
#include <utility>

bool PrivilegeManager::EnableSymlinkPrivilege() {
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

bool PathInspector::PathExists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) || fs::is_symlink(p, ec);
}

bool PathInspector::IsDirectoryPath(const fs::path& p, bool no_deref) {
    std::error_code ec;
    if (no_deref && fs::is_symlink(p, ec)) {
        return false;
    }
    return fs::is_directory(p, ec);
}

bool ConsolePrompter::ConfirmOverwrite(const fs::path& target) {
    std::wcout << L"ln: replace '" << target.wstring() << L"'? (y/N) ";
    std::wstring resp;
    if (std::getline(std::wcin, resp)) {
        if (!resp.empty() && (resp[0] == L'y' || resp[0] == L'Y')) {
            return true;
        }
    }
    return false;
}

LinkEngine::LinkEngine(LnOptions opts) : m_opts(std::move(opts)) {}

bool LinkEngine::CreateOneLink(const fs::path& source, const fs::path& target) const {
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
