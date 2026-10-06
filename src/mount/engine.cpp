#include "engine.hpp"

bool AdminValidator::IsRunningAsAdmin() {
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

std::wstring SystemErrorResolver::GetSystemErrorMessage(DWORD errorCode) {
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

std::wstring StringUtils::TrimQuotes(const std::wstring& str) {
    if (str.length() >= 2 && str.front() == L'"' && str.back() == L'"') {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

void MountLister::ListMounts(bool verbose) {
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

bool MountEngine::Unmount(std::wstring target) {
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

bool MountEngine::MountNetwork(const std::wstring& remote, const std::wstring& target, const std::wstring& user, const std::wstring& pass) {
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

bool MountEngine::MountVolume(std::wstring volumeGuid, std::wstring targetFolder) {
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
