// ============================================================================
// 3. MOUNT INSPECTION ENGINE
// ============================================================================

class MountInspector {
public:
    static std::wstring RemoteShareForDrive(const std::wstring& driveRoot) {
        if (driveRoot.size() < 2 || driveRoot[1] != L':') {
            return L"";
        }

        wchar_t localName[3] = { driveRoot[0], L':', L'\0' };
        wchar_t remoteName[MAX_PATH] = {};
        DWORD remoteLen = MAX_PATH;
        DWORD result = WNetGetConnectionW(localName, remoteName, &remoteLen);
        return (result == NO_ERROR) ? remoteName : L"";
    }

    static std::wstring JoinOptions(UINT driveType, DWORD volumeFlags) {
        std::wstring options = (driveType == DRIVE_CDROM || (volumeFlags & FILE_READ_ONLY_VOLUME)) ? L"ro" : L"rw";
        switch (driveType) {
            case DRIVE_REMOTE:    options += L",network"; break;
            case DRIVE_CDROM:     options += L",cdrom"; break;
            case DRIVE_REMOVABLE: options += L",removable"; break;
            default:              options += L",local"; break;
        }
        return options;
    }

    static MountEntry BuildEntry(const std::wstring& driveRoot) {
        MountEntry entry;
        entry.target = driveRoot;
        entry.source = driveRoot.size() >= 2 ? driveRoot.substr(0, 2) : driveRoot;

        UINT driveType = GetDriveTypeW(driveRoot.c_str());
        if (driveType == DRIVE_REMOTE) {
            std::wstring remote = RemoteShareForDrive(driveRoot);
            if (!remote.empty()) {
                entry.source = remote;
            }
        } else {
            wchar_t guidPath[MAX_PATH] = {};
            if (GetVolumeNameForVolumeMountPointW(driveRoot.c_str(), guidPath, MAX_PATH)) {
                entry.source = guidPath;
            }
        }

        wchar_t fsName[MAX_PATH] = {};
        DWORD volumeFlags = 0;
        if (GetVolumeInformationW(driveRoot.c_str(), nullptr, 0, nullptr, nullptr, &volumeFlags, fsName, MAX_PATH)) {
            entry.fsType = fsName;
        } else {
            entry.fsType = L"unknown";
        }

        entry.options = JoinOptions(driveType, volumeFlags);
        return entry;
    }

    static std::vector<MountEntry> EnumerateMounts() {
        std::vector<MountEntry> entries;
        DWORD length = GetLogicalDriveStringsW(0, nullptr);
        if (length == 0) {
            return entries;
        }

        std::vector<wchar_t> buffer(length + 1, L'\0');
        if (GetLogicalDriveStringsW(length, buffer.data()) == 0) {
            return entries;
        }

        wchar_t* drive = buffer.data();
        while (*drive != L'\0') {
            entries.push_back(BuildEntry(drive));
            drive += wcslen(drive) + 1;
        }

        return entries;
    }
};
