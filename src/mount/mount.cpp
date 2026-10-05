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

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <winnetwk.h>
#include <virtdisk.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "mpr.lib")
#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "Advapi32.lib")

// ============================================================================
// 1. ADMIN VALIDATOR, ERROR RESOLVER & STRING UTILS
// ============================================================================

class AdminValidator {
public:
    static bool IsRunningAsAdmin() {
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        PSID adminGroup = nullptr;

        if (!AllocateAndInitializeSid(
                &ntAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &adminGroup)) {
            return false;
        }

        BOOL isAdmin = FALSE;
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);

        return isAdmin == TRUE;
    }
};

class SystemErrorResolver {
public:
    static std::wstring GetSystemErrorMessage(DWORD errorCode) {
        LPWSTR buf = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&buf, 0, NULL
        );
        std::wstring msg = (size && buf) ? buf : L"Unknown error.";
        if (buf) LocalFree(buf);
        while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n')) {
            msg.pop_back();
        }
        return msg;
    }
};

class StringUtils {
public:
    static std::wstring TrimQuotes(const std::wstring& str) {
        if (str.length() >= 2 && str.front() == L'"' && str.back() == L'"') {
            return str.substr(1, str.length() - 2);
        }
        return str;
    }
};

// ============================================================================
// 2. MOUNT LISTER & MOUNT ENGINE
// ============================================================================

class MountLister {
public:
    static void ListMounts(bool verbose) {
        DWORD drivesMask = GetLogicalDrives();
        wchar_t driveBuffer[] = L"A:\\";

        for (char i = 0; i < 26; ++i) {
            if (drivesMask & (1 << i)) {
                driveBuffer[0] = L'A' + i;

                wchar_t volumeName[MAX_PATH] = { 0 };
                wchar_t fsName[MAX_PATH] = { 0 };
                DWORD serialNumber = 0, maxComponent = 0, flags = 0;

                GetVolumeInformationW(
                    driveBuffer, volumeName, MAX_PATH,
                    &serialNumber, &maxComponent, &flags, fsName, MAX_PATH
                );

                UINT driveType = GetDriveTypeW(driveBuffer);
                std::wstring typeStr = L"unknown";
                std::vector<std::wstring> options;

                switch (driveType) {
                    case DRIVE_REMOVABLE: typeStr = L"removable"; options.push_back(L"local"); break;
                    case DRIVE_FIXED:     typeStr = L"fixed";     options.push_back(L"local"); break;
                    case DRIVE_REMOTE:    typeStr = L"smbfs";     options.push_back(L"network"); break;
                    case DRIVE_CDROM:     typeStr = L"cd9660";    options.push_back(L"read-only"); break;
                    case DRIVE_RAMDISK:   typeStr = L"ramdisk";   options.push_back(L"local"); break;
                }

                if (flags & FILE_READ_ONLY_VOLUME) options.push_back(L"read-only");
                else options.push_back(L"read/write");

                wchar_t deviceGuid[MAX_PATH] = { 0 };
                GetVolumeNameForVolumeMountPointW(driveBuffer, deviceGuid, MAX_PATH);

                std::wcout << (deviceGuid[0] ? deviceGuid : driveBuffer)
                           << L" on " << driveBuffer[0] << L":"
                           << L" (" << (fsName[0] ? fsName : typeStr.c_str()) << L", ";

                for (size_t o = 0; o < options.size(); ++o) {
                    std::wcout << options[o] << (o + 1 < options.size() ? L", " : L"");
                }
                std::wcout << L")\n";

                if (verbose && deviceGuid[0]) {
                    wchar_t mountPoint[MAX_PATH] = { 0 };
                    HANDLE hEnum = FindFirstVolumeMountPointW(deviceGuid, mountPoint, MAX_PATH);
                    if (hEnum != INVALID_HANDLE_VALUE) {
                        do {
                            std::wcout << L"  -> mounted on subfolder: " << driveBuffer << mountPoint << L"\n";
                        } while (FindNextVolumeMountPointW(hEnum, mountPoint, MAX_PATH));
                        FindVolumeMountPointClose(hEnum);
                    }
                }
            }
        }
    }
};

class MountEngine {
public:
    static bool Unmount(std::wstring target) {
        target = StringUtils::TrimQuotes(target);
        if (target.empty()) return false;

        if (target.length() == 2 && target[1] == L':') {
            DWORD res = WNetCancelConnection2W(target.c_str(), CONNECT_UPDATE_PROFILE, TRUE);
            if (res == NO_ERROR) {
                std::wcout << L"Successfully unmounted network connection " << target << L"\n";
                return true;
            }
        }

        if (target.back() != L'\\') target += L'\\';

        if (DeleteVolumeMountPointW(target.c_str())) {
            std::wcout << L"Successfully unmounted " << target << L"\n";
            return true;
        } else {
            DWORD err = GetLastError();
            std::wcerr << L"mount: unmount of " << target << L" failed: " << SystemErrorResolver::GetSystemErrorMessage(err) << L"\n";
            return false;
        }
    }

    static bool MountNetwork(const std::wstring& remote, const std::wstring& target, const std::wstring& user, const std::wstring& pass) {
        NETRESOURCEW nr = { 0 };
        nr.dwType = RESOURCETYPE_DISK;
        nr.lpLocalName = const_cast<LPWSTR>(target.c_str());
        nr.lpRemoteName = const_cast<LPWSTR>(remote.c_str());

        DWORD res = WNetAddConnection2W(
            &nr,
            pass.empty() ? NULL : pass.c_str(),
            user.empty() ? NULL : user.c_str(),
            CONNECT_TEMPORARY
        );

        if (res == NO_ERROR) {
            std::wcout << remote << L" mounted successfully on " << target << L"\n";
            return true;
        } else {
            std::wcerr << L"mount: network mount failed: " << SystemErrorResolver::GetSystemErrorMessage(res) << L"\n";
            return false;
        }
    }

    static bool MountVolume(std::wstring volumeGuid, std::wstring targetFolder) {
        if (volumeGuid.back() != L'\\') volumeGuid += L'\\';
        if (targetFolder.back() != L'\\') targetFolder += L'\\';

        if (SetVolumeMountPointW(targetFolder.c_str(), volumeGuid.c_str())) {
            std::wcout << volumeGuid << L" mounted on " << targetFolder << L"\n";
            return true;
        } else {
            DWORD err = GetLastError();
            std::wcerr << L"mount: volume mount failed: " << SystemErrorResolver::GetSystemErrorMessage(err) << L"\n";
            return false;
        }
    }
};

// ============================================================================
// 3. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct MountOptions {
    std::wstring fsType;
    std::wstring options;
    std::wstring username;
    std::wstring password;
    bool unmountMode = false;
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;
    std::vector<std::wstring> positionalArgs;
};

class OptionParser {
public:
    static void ShowUsage() {
           std::wcout << LR"HELP(mount(1)                 CrossShell for UNIX Reference Manual                    mount(1)

    NAME
        mount - list, mount, and unmount Windows file systems

    SYNOPSIS
        mount
        mount -v
        mount -u TARGET
        mount -t smbfs [-o OPTIONS] //server/share DRIVE:
        mount VOLUME_GUID FOLDER

    DESCRIPTION
        Lists mounted file systems or mounts and unmounts Windows volumes, UNC
        shares, SMB/CIFS paths, and volume GUIDs.

    OPTIONS
        -t, --types TYPE       Select smbfs, cifs, or ntfs handling.
        -o, --options OPTIONS  Comma-separated user/password mount options.
        -u, -d, --umount       Unmount TARGET.
        -v, --verbose          Include mounted subfolder details.
        -h, /?, --help         Display this comprehensive reference manual.
        --version              Display version information and exit.
        --                     End options.

    EXAMPLES
        mount
        mount -v
        mount -u E:
        mount -t smbfs -o user=admin,pass=secret //server/share E:
        mount \\?\Volume{GUID} C:\Mount\Data

    EXIT STATUS
        0          Help, version, listing, or successful mount/unmount.
        1          Parse, privilege, unsupported-device, or Windows operation failure.

    CrossShell for UNIX                                                        mount(1)
    )HELP";
           return;

        std::wcout << L"Usage:\n"
                   << L"  mount                             List all mounted file systems\n"
                   << L"  mount -v                          List file systems verbosely\n"
                   << L"  mount -u <target>                 Unmount drive letter or folder\n"
                   << L"  mount -t smbfs [-o user=U,pass=P] //server/share Drive:\n"
                   << L"  mount <VolumeGUID> <FolderDir>    Mount Volume GUID to an empty NTFS directory\n\n"
                   << L"Options:\n"
                   << L"  -t <type>, --types <type>        File system type (smbfs, cifs, ntfs)\n"
                   << L"  -o <options>, --options <opts>   Comma-separated options (e.g., user=admin,pass=1234)\n"
                   << L"  -u, -d, --umount                 Unmount specified target point\n"
                   << L"  -v, --verbose                    Verbose mode\n"
                   << L"  -h, --help                       Show this help text\n"
                   << L"      --version                    Show version information\n"
                   << L"      --                           End of options\n";
    }

    static void ShowVersion() {
        std::wcout << L"mount 1.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], MountOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"--") {
                for (int j = i + 1; j < argc; ++j) {
                    opts.positionalArgs.push_back(argv[j]);
                }
                break;
            } else if (arg == L"-h" || arg == L"/?" || arg == L"--help") {
                opts.showHelp = true;
                return true;
            } else if (arg == L"--version") {
                opts.showVersion = true;
                return true;
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-u" || arg == L"-d" || arg == L"--umount") {
                opts.unmountMode = true;
            } else if ((arg == L"-t" || arg == L"--types") && i + 1 < argc) {
                opts.fsType = argv[++i];
            } else if ((arg == L"-o" || arg == L"--options") && i + 1 < argc) {
                opts.options = argv[++i];

                std::wstringstream ss(opts.options);
                std::wstring token;
                while (std::getline(ss, token, L',')) {
                    std::wstring trimmed = token;
                    auto is_space = [](wchar_t ch) { return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n'; };
                    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [&](wchar_t ch) { return !is_space(ch); }));
                    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [&](wchar_t ch) { return !is_space(ch); }).base(), trimmed.end());
                    if (trimmed.empty()) continue;
                    size_t eq = trimmed.find(L'=');
                    if (eq != std::wstring::npos) {
                        std::wstring key = trimmed.substr(0, eq);
                        std::wstring val = trimmed.substr(eq + 1);
                        if (key == L"user" || key == L"username") opts.username = val;
                        if (key == L"pass" || key == L"password") opts.password = val;
                    }
                }
            } else {
                opts.positionalArgs.push_back(arg);
            }
        }
        return true;
    }
};

class MountApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        MountOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return 1;
        }

        if (opts.showHelp) {
            OptionParser::ShowUsage();
            return 0;
        }

        if (opts.showVersion) {
            OptionParser::ShowVersion();
            return 0;
        }

        if (!AdminValidator::IsRunningAsAdmin()) {
            std::wcerr << L"mount: this command requires administrator privileges. Run from an elevated terminal.\n";
            return 1;
        }

        // 1. List Mode
        if (opts.positionalArgs.empty() && !opts.unmountMode) {
            MountLister::ListMounts(opts.verbose);
            return 0;
        }

        // 2. Unmount Mode
        if (opts.unmountMode) {
            if (opts.positionalArgs.empty()) {
                std::wcerr << L"mount: missing target to unmount.\n";
                return 1;
            }
            return MountEngine::Unmount(opts.positionalArgs[0]) ? 0 : 1;
        }

        // 3. Mount Mode
        if (opts.positionalArgs.size() < 2) {
            std::wcerr << L"mount: missing source device or target mount point.\n";
            OptionParser::ShowUsage();
            return 1;
        }

        std::wstring source = StringUtils::TrimQuotes(opts.positionalArgs[0]);
        std::wstring target = StringUtils::TrimQuotes(opts.positionalArgs[1]);

        if (opts.fsType == L"smbfs" || opts.fsType == L"cifs" || source.rfind(L"\\\\", 0) == 0 || source.rfind(L"//", 0) == 0) {
            std::replace(source.begin(), source.end(), L'/', L'\\');
            return MountEngine::MountNetwork(source, target, opts.username, opts.password) ? 0 : 1;
        }

        if (source.rfind(L"\\\\?\\Volume", 0) == 0) {
            return MountEngine::MountVolume(source, target) ? 0 : 1;
        }

        std::wcerr << L"mount: unsupported device or filesystem type specified.\n";
        return 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    MountApplication app;
    return app.Run(argc, argv);
}
