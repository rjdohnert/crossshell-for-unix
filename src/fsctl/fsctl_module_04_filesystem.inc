// ============================================================================
// 4. FILE SYSTEM OPERATIONS ENGINE
// ============================================================================

class SystemInspector {
public:
    static bool IsCurrentUserAdministrator() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(
                &ntAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &adminGroup)) {
            ScopedSid scopedSid(adminGroup);
            CheckTokenMembership(nullptr, scopedSid.Get(), &isAdmin);
        }

        return isAdmin == TRUE;
    }

    static void ListDrives() {
        DWORD driveMask = GetLogicalDrives();
        if (driveMask == 0) {
            std::cout << "\033[1;31mError listing drives (code " << GetLastError() << ").\033[0m\n";
            return;
        }

        std::cout << "\n\033[1;36mAvailable drives:\033[0m\n";
        std::cout << "-------------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(8) << "Drive"
                  << std::setw(12) << "Type"
                  << std::setw(12) << "Filesystem"
                  << std::setw(16) << "Total Space"
                  << std::setw(16) << "Free Space"
                  << "Label\n";
        std::cout << "-------------------------------------------------------------------------------------\n";

        for (int index = 0; index < 26; ++index) {
            if ((driveMask & (1u << index)) == 0) continue;

            wchar_t rootPath[] = L"A:\\";
            rootPath[0] = static_cast<wchar_t>(L'A' + index);

            const char* driveTypeName = "Unknown";
            switch (GetDriveTypeW(rootPath)) {
                case DRIVE_FIXED: driveTypeName = "Fixed"; break;
                case DRIVE_REMOVABLE: driveTypeName = "Removable"; break;
                case DRIVE_CDROM: driveTypeName = "CD-ROM"; break;
                case DRIVE_REMOTE: driveTypeName = "Network"; break;
                case DRIVE_RAMDISK: driveTypeName = "RAM Disk"; break;
                case DRIVE_NO_ROOT_DIR: driveTypeName = "Invalid"; break;
            }

            wchar_t volumeName[MAX_PATH] = L"";
            wchar_t fileSystemName[MAX_PATH] = L"";
            if (!GetVolumeInformationW(rootPath, volumeName, MAX_PATH, NULL, NULL,
                                       NULL, fileSystemName, MAX_PATH)) {
                volumeName[0] = L'\0';
                fileSystemName[0] = L'\0';
            }

            ULARGE_INTEGER freeBytesAvailable = {0};
            ULARGE_INTEGER totalNumberOfBytes = {0};
            ULARGE_INTEGER totalNumberOfFreeBytes = {0};
            std::wstring totalSpaceStr = L"-";
            std::wstring freeSpaceStr = L"-";

            if (GetDiskFreeSpaceExW(rootPath, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
                totalSpaceStr = StringHelper::FormatBytes(totalNumberOfBytes.QuadPart);
                freeSpaceStr = StringHelper::FormatBytes(freeBytesAvailable.QuadPart);
            }

            std::string colorCode = "\033[0m";
            if (GetDriveTypeW(rootPath) == DRIVE_FIXED) {
                colorCode = "\033[1;32m";
            } else if (GetDriveTypeW(rootPath) == DRIVE_REMOTE || GetDriveTypeW(rootPath) == DRIVE_RAMDISK) {
                colorCode = "\033[1;36m";
            } else {
                colorCode = "\033[90m";
            }

            std::cout << colorCode << std::left << std::setw(8) << StringHelper::ToNarrow(rootPath) << "\033[0m"
                      << std::left << std::setw(12) << driveTypeName
                      << std::setw(12) << (fileSystemName[0] != L'\0' ? StringHelper::ToNarrow(fileSystemName) : "-")
                      << std::setw(16) << StringHelper::ToNarrow(totalSpaceStr)
                      << std::setw(16) << StringHelper::ToNarrow(freeSpaceStr)
                      << (volumeName[0] != L'\0' ? StringHelper::ToNarrow(volumeName) : "-")
                      << "\n";
        }

        std::cout << "-------------------------------------------------------------------------------------\n";
    }

    static void ListNetworkShares() {
        std::cout << "\n\033[1;36mAvailable Network Shares:\033[0m\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(40) << "Share Path" << "Type\n";
        std::cout << "----------------------------------------------------------------------\n";

        ScopedEnumHandle hEnum;
        NETRESOURCEW nr = {0};
        nr.dwType = RESOURCETYPE_DISK;
        nr.dwScope = RESOURCE_GLOBALNET;
        nr.dwUsage = RESOURCEUSAGE_CONNECTABLE;

        DWORD dwResult = WNetOpenEnumW(RESOURCE_GLOBALNET, RESOURCETYPE_DISK, RESOURCEUSAGE_ALL, NULL, hEnum.AddressOf());
        if (dwResult != NO_ERROR) {
            std::cout << "\033[1;31mUnable to enumerate network resources. Error: " << dwResult << "\033[0m\n";
            std::cout << "----------------------------------------------------------------------\n";
            return;
        }

        DWORD cbBuffer = 16384;
        LPNETRESOURCEW lpnrs = (LPNETRESOURCEW)GlobalAlloc(GPTR, cbBuffer);
        if (lpnrs == NULL) {
            std::cout << "\033[1;31mMemory allocation failed.\033[0m\n";
            return;
        }

        bool foundShares = false;
        do {
            DWORD cEntries = (DWORD)-1;
            dwResult = WNetEnumResourceW(hEnum.Get(), &cEntries, lpnrs, &cbBuffer);
            if (dwResult == NO_ERROR) {
                for (DWORD i = 0; i < cEntries; ++i) {
                    foundShares = true;
                    std::string remotePath = "";
                    std::string remoteType = "";

                    if (lpnrs[i].lpRemoteName) {
                        remotePath = StringHelper::ToNarrow(lpnrs[i].lpRemoteName);
                    }

                    switch (lpnrs[i].dwType) {
                        case RESOURCETYPE_DISK:
                            remoteType = "Disk";
                            break;
                        case RESOURCETYPE_PRINT:
                            remoteType = "Printer";
                            break;
                        default:
                            remoteType = "Other";
                            break;
                    }

                    std::cout << std::left << std::setw(40) << remotePath << remoteType << "\n";
                }
            } else if (dwResult != ERROR_NO_MORE_ITEMS) {
                break;
            }
        } while (dwResult == NO_ERROR);

        GlobalFree((HGLOBAL)lpnrs);

        if (!foundShares) {
            std::cout << "  <No network shares available>\n";
        }

        std::cout << "----------------------------------------------------------------------\n";
    }
};

class TreeRenderer {
public:
    static void PrintTreeRecursive(const std::wstring& baseDir, const std::wstring& indent, bool isLast,
                                   unsigned int& fileCount, unsigned int& dirCount, unsigned long long& totalSize) {
        std::wstring searchPath = PathHelper::JoinPath(baseDir, L"*");
        WIN32_FIND_DATAW findData;
        ScopedFindFileHandle findHandle(FindFirstFileW(searchPath.c_str(), &findData));
        if (!findHandle.IsValid()) return;

        std::vector<TreeItem> items;
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            items.push_back({ findData.cFileName, (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 });

            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                fileCount++;
                unsigned long long fileSize = (static_cast<unsigned long long>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
                totalSize += fileSize;
            } else {
                dirCount++;
            }
        } while (FindNextFileW(findHandle.Get(), &findData) != 0);

        std::sort(items.begin(), items.end(), [](const TreeItem& a, const TreeItem& b) {
            return a.name < b.name;
        });

        for (size_t i = 0; i < items.size(); ++i) {
            bool itemIsLast = (i == items.size() - 1);
            std::string marker = itemIsLast ? "\\-- " : "|-- ";
            std::string colorPrefix = items[i].isDir ? "\033[1;36m" : "\033[0m";
            std::string colorSuffix = "\033[0m";

            std::cout << StringHelper::ToNarrow(indent) << marker << colorPrefix << StringHelper::ToNarrow(items[i].name) << colorSuffix << "\n";

            if (items[i].isDir) {
                std::wstring nextIndent = indent + (itemIsLast ? L"    " : L"|   ");
                PrintTreeRecursive(PathHelper::JoinPath(baseDir, items[i].name), nextIndent, itemIsLast, fileCount, dirCount, totalSize);
            }
        }
    }

    static void RunTree(const std::wstring& baseDir) {
        std::cout << "\033[1;36m" << StringHelper::ToNarrow(baseDir) << "\033[0m\n";
        unsigned int fileCount = 0;
        unsigned int dirCount = 0;
        unsigned long long totalSize = 0;

        PrintTreeRecursive(baseDir, L"", true, fileCount, dirCount, totalSize);

        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "Summary: " << fileCount << " File(s), " << dirCount << " Dir(s), Total size: " << StringHelper::ToNarrow(StringHelper::FormatBytes(totalSize)) << "\n";
        std::cout << "----------------------------------------------------------------------\n";
    }
};

class FileOperator {
public:
    static std::wstring GetWindowsModeString(DWORD attributes, const std::wstring& fullPath = L"") {
        std::wstring mode = L"---------";
        if (attributes & FILE_ATTRIBUTE_DIRECTORY)     mode[0] = L'd';
        if (attributes & FILE_ATTRIBUTE_READONLY)      mode[1] = L'r';
        if (attributes & FILE_ATTRIBUTE_ARCHIVE)       mode[2] = L'a';
        if (attributes & FILE_ATTRIBUTE_HIDDEN)        mode[3] = L'h';
        if (attributes & FILE_ATTRIBUTE_SYSTEM)        mode[4] = L's';
        if (attributes & FILE_ATTRIBUTE_COMPRESSED)    mode[5] = L'c';
        if (attributes & FILE_ATTRIBUTE_ENCRYPTED)     mode[6] = L'e';
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) mode[7] = L'l';

        if (!fullPath.empty()) {
            PSECURITY_DESCRIPTOR pSD = nullptr;
            PACL pDacl = nullptr;
            if (GetNamedSecurityInfoW(fullPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                     nullptr, nullptr, &pDacl, nullptr, &pSD) == ERROR_SUCCESS) {
                if (pDacl != nullptr && pDacl->AceCount > 0) {
                    mode[8] = L'+';
                }
                if (pSD) LocalFree(pSD);
            }
        }
        return mode;
    }

    static void ClassifyFile(const std::wstring& wname, bool isDir, bool& isExecutable, bool& isSource, bool& isImage) {
        isExecutable = false;
        isSource = false;
        isImage = false;
        if (isDir) return;

        size_t dot = wname.find_last_of(L'.');
        if (dot == std::wstring::npos) return;

        std::wstring ext = wname.substr(dot);
        for (auto& ch : ext) ch = std::towlower(ch);

        if (ext == L".exe" || ext == L".bat" || ext == L".cmd" || ext == L".ps1" || ext == L".com" || ext == L".msi" || ext == L".vbs") {
            isExecutable = true;
        } else if (ext == L".c" || ext == L".cpp" || ext == L".cxx" || ext == L".cc" || ext == L".h" || ext == L".hpp" ||
                   ext == L".cs" || ext == L".rs" || ext == L".go" || ext == L".py" || ext == L".java" || ext == L".js" ||
                   ext == L".ts" || ext == L".html" || ext == L".css" || ext == L".sql" || ext == L".sh" || ext == L".asm" ||
                   ext == L".json" || ext == L".xml" || ext == L".yaml" || ext == L".yml" || ext == L".toml" || ext == L".lua" ||
                   ext == L".spec" || ext == L".md" || ext == L".php" || ext == L".rb" || ext == L".swift" || ext == L".kt" ||
                   ext == L".dart" || ext == L".r" || ext == L".pl" || ext == L".cmake" || ext == L".txt") {
            isSource = true;
        } else if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".gif" || ext == L".bmp" || ext == L".svg" ||
                   ext == L".webp" || ext == L".ico" || ext == L".tiff" || ext == L".tif" || ext == L".psd" || ext == L".raw" ||
                   ext == L".heic" || ext == L".avif") {
            isImage = true;
        }
    }

    static void ListDirectory(const std::wstring& path, bool showHidden = false) {
        if (RemoteProtocolHelper::IsRemotePath(path)) {
            RemoteInspector::ListRemoteDirectory(path);
            return;
        }

        std::cout << "\nContents of \033[1;36m" << StringHelper::ToNarrow(path) << "\033[0m:\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(12) << "Mode"
                  << std::setw(8) << "Type"
                  << std::setw(44) << "Name"
                  << std::right << std::setw(16) << "Size\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        WIN32_FIND_DATAW findData;
        ScopedFindFileHandle findHandle(FindFirstFileW(PathHelper::JoinPath(path, L"*").c_str(), &findData));
        if (!findHandle.IsValid()) {
            std::cout << "\033[1;31mError reading directory.\033[0m\n";
            std::cout << "--------------------------------------------------------------------------------\n";
            return;
        }

        size_t fileCount = 0;
        size_t dirCount = 0;
        unsigned long long totalFilesSize = 0;
        bool foundAny = false;

        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }

            std::wstring wname = findData.cFileName;
            bool isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0 || (!wname.empty() && wname[0] == L'.');
            if (isHidden && !showHidden) {
                continue;
            }

            foundAny = true;
            std::string name = StringHelper::ToNarrow(wname);
            std::wstring fullPath = PathHelper::JoinPath(path, wname);
            std::string modeStr = StringHelper::ToNarrow(GetWindowsModeString(findData.dwFileAttributes, fullPath));

            int namePad = 44 - static_cast<int>(wname.length());
            if (namePad < 1) namePad = 1;
            std::string padSpaces(static_cast<size_t>(namePad), ' ');

            bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool isExecutable = false, isSource = false, isImage = false;
            ClassifyFile(wname, isDir, isExecutable, isSource, isImage);

            if (isDir) {
                dirCount++;
                std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                          << "\033[1;36m[DIR]   \033[0m"
                          << "\033[1;36m" << name << "\033[0m" << padSpaces
                          << std::right << std::setw(16) << "--\n";
            } else {
                fileCount++;
                unsigned long long size = (static_cast<unsigned long long>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
                totalFilesSize += size;
                std::string sizeText = StringHelper::ToNarrow(StringHelper::FormatBytes(size));

                if (isExecutable) {
                    std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                              << "\033[1;32m[EXE]   \033[0m"
                              << "\033[1;32m" << name << "\033[0m" << padSpaces
                              << std::right << std::setw(16) << sizeText << "\n";
                } else if (isSource) {
                    std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                              << "\033[38;2;255;140;0m[SRC]   \033[0m"
                              << "\033[38;2;255;140;0m" << name << "\033[0m" << padSpaces
                              << std::right << std::setw(16) << sizeText << "\n";
                } else if (isImage) {
                    std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                              << "\033[38;2;255;105;180m[IMG]   \033[0m"
                              << "\033[38;2;255;105;180m" << name << "\033[0m" << padSpaces
                              << std::right << std::setw(16) << sizeText << "\n";
                } else {
                    std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                              << "FILE    "
                              << name << padSpaces
                              << std::right << std::setw(16) << sizeText << "\n";
                }
            }
        } while (FindNextFileW(findHandle.Get(), &findData) != 0);

        if (!foundAny) {
            std::cout << "  <Directory is empty>\n";
        }

        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << "Summary: " << fileCount << " File(s), " << dirCount << " Dir(s), Total size: " << StringHelper::ToNarrow(StringHelper::FormatBytes(totalFilesSize)) << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";
    }

    static void CatFile(const std::wstring& path) {
        if (RemoteProtocolHelper::IsRemotePath(path)) {
            RemoteInspector::CatRemoteFile(path);
            return;
        }

        ScopedFileHandle hFile(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
        if (!hFile.IsValid()) {
            std::cout << "\033[1;31mError opening file: " << StringHelper::ToNarrow(path) << " (code " << GetLastError() << ")\033[0m\n";
            return;
        }

        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(hFile.Get(), buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::cout << buffer;
        }
        std::cout << "\n";
    }

    static void StatPath(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA attrData;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrData)) {
            std::cout << "\033[1;31mError getting attributes for: " << StringHelper::ToNarrow(path) << " (code " << GetLastError() << ")\033[0m\n";
            return;
        }

        std::cout << "\n\033[1;36mFile Details:\033[0m " << StringHelper::ToNarrow(path) << "\n";
        std::cout << "-----------------------------------------------------------------\n";

        bool isDir = (attrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        std::cout << "Type:             " << (isDir ? "\033[1;36mDirectory\033[0m" : "File") << "\n";
        std::cout << "Mode:             " << StringHelper::ToNarrow(GetWindowsModeString(attrData.dwFileAttributes, path)) << " (Windows Native)\n";

        unsigned long long size = (static_cast<unsigned long long>(attrData.nFileSizeHigh) << 32) | attrData.nFileSizeLow;
        std::cout << "Size:             " << StringHelper::ToNarrow(StringHelper::FormatBytes(size)) << " (" << size << " bytes)\n";

        std::cout << "Created:          " << StringHelper::ToNarrow(PathHelper::FileTimeToString(attrData.ftCreationTime)) << "\n";
        std::cout << "Last Access:      " << StringHelper::ToNarrow(PathHelper::FileTimeToString(attrData.ftLastAccessTime)) << "\n";
        std::cout << "Last Modify:      " << StringHelper::ToNarrow(PathHelper::FileTimeToString(attrData.ftLastWriteTime)) << "\n";

        std::cout << "Attributes:       ";
        std::vector<std::string> attrs;
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) attrs.push_back("Read-Only");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) attrs.push_back("Hidden");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) attrs.push_back("System");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) attrs.push_back("Directory");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) attrs.push_back("Archive");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) attrs.push_back("Compressed");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) attrs.push_back("Encrypted");

        for (size_t i = 0; i < attrs.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << attrs[i];
        }
        if (attrs.empty()) std::cout << "None";
        std::cout << "\n";
        std::cout << "-----------------------------------------------------------------\n";
    }

    static void FindFilesRecursive(const std::wstring& baseDir, const std::wstring& pattern, std::vector<std::wstring>& results) {
        WIN32_FIND_DATAW findData;
        std::wstring searchPath = PathHelper::JoinPath(baseDir, L"*");
        ScopedFindFileHandle findHandle(FindFirstFileW(searchPath.c_str(), &findData));
        if (!findHandle.IsValid()) return;

        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }

            std::wstring childName = findData.cFileName;
            std::wstring fullPath = PathHelper::JoinPath(baseDir, childName);

            std::wstring lowerChild = childName;
            std::wstring lowerPattern = pattern;
            for (auto& c : lowerChild) c = std::towlower(c);
            for (auto& c : lowerPattern) c = std::towlower(c);

            if (lowerChild.find(lowerPattern) != std::wstring::npos) {
                results.push_back(fullPath);
            }

            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                FindFilesRecursive(fullPath, pattern, results);
            }
        } while (FindNextFileW(findHandle.Get(), &findData) != 0);
    }

    static void RunFind(const std::wstring& baseDir, const std::wstring& pattern) {
        std::cout << "\nSearching for \"" << StringHelper::ToNarrow(pattern) << "\" starting from " << StringHelper::ToNarrow(baseDir) << "...\n";
        std::vector<std::wstring> results;
        FindFilesRecursive(baseDir, pattern, results);

        if (results.empty()) {
            std::cout << "\033[1;33mNo matches found.\033[0m\n";
            return;
        }

        std::cout << "\033[1;32mFound " << results.size() << " match(es):\033[0m\n";
        for (const auto& path : results) {
            if (PathHelper::IsDirectoryPath(path)) {
                std::cout << "  \033[1;36m" << StringHelper::ToNarrow(path) << "/\033[0m\n";
            } else {
                std::cout << "  " << StringHelper::ToNarrow(path) << "\n";
            }
        }
    }

    static bool ConfirmDestructiveAction(const std::wstring& actionDescription) {
        std::cout << "\033[1;33mConfirm " << StringHelper::ToNarrow(actionDescription) << "? [y/N]: \033[0m";
        std::string response;
        if (!std::getline(std::cin, response)) {
            return false;
        }
        std::wstring wresponse = StringHelper::ToWide(response);
        std::transform(wresponse.begin(), wresponse.end(), wresponse.begin(), [](wchar_t c) {
            return std::towlower(c);
        });
        return wresponse == L"y" || wresponse == L"yes";
    }

    static void TouchPath(const std::wstring& path, bool noCreate) {
        if (RemoteProtocolHelper::IsRemotePath(path)) {
            std::cout << "\033[1;31mError: touch is supported for local paths only.\033[0m\n";
            return;
        }

        bool existed = PathHelper::PathExists(path);
        if (existed && PathHelper::IsDirectoryPath(path)) {
            std::cout << "\033[1;31mError: Target is a directory: " << StringHelper::ToNarrow(path) << "\033[0m\n";
            return;
        }

        if (!existed && noCreate) {
            std::cout << "\033[90mSkipped (missing): " << StringHelper::ToNarrow(path) << "\033[0m\n";
            return;
        }

        if (!existed && !PathHelper::EnsureParentDirectoryExists(path)) {
            std::cout << "\033[1;31mError: Unable to create parent directory for: " << StringHelper::ToNarrow(path) << "\033[0m\n";
            return;
        }

        ScopedFileHandle hFile(CreateFileW(
            path.c_str(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        ));

        if (!hFile.IsValid()) {
            std::cout << "\033[1;31mError touching file (code " << GetLastError() << ").\033[0m\n";
            return;
        }

        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        if (!SetFileTime(hFile.Get(), nullptr, nullptr, &now)) {
            DWORD err = GetLastError();
            std::cout << "\033[1;31mError updating timestamp (code " << err << ").\033[0m\n";
            return;
        }

        std::cout << "\033[1;32m" << (existed ? "Timestamp updated: " : "File created: ") << StringHelper::ToNarrow(path) << "\033[0m\n";
    }

    static bool RemoveDirectoryTree(const std::wstring& path, uintmax_t& removedCount) {
        std::error_code ec;
        removedCount = std::filesystem::remove_all(path, ec);
        return !ec;
    }

    static bool ReadTextFileLines(const std::wstring& path, std::vector<std::string>& lines, std::string& error) {
        lines.clear();
        error.clear();

        std::ifstream file{ std::filesystem::path(path) };
        if (!file.is_open()) {
            error = "Unable to open file.";
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }

        if (file.bad()) {
            error = "I/O error while reading file.";
            return false;
        }

        return true;
    }

    static void ShowFileDiff(const std::wstring& leftPath, const std::wstring& rightPath) {
        if (RemoteProtocolHelper::IsRemotePath(leftPath) || RemoteProtocolHelper::IsRemotePath(rightPath)) {
            std::cout << "\033[1;31mError: diff is supported for local files only.\033[0m\n";
            return;
        }

        if (!PathHelper::PathExists(leftPath) || PathHelper::IsDirectoryPath(leftPath)) {
            std::cout << "\033[1;31mError: Left file is invalid: " << StringHelper::ToNarrow(leftPath) << "\033[0m\n";
            return;
        }
        if (!PathHelper::PathExists(rightPath) || PathHelper::IsDirectoryPath(rightPath)) {
            std::cout << "\033[1;31mError: Right file is invalid: " << StringHelper::ToNarrow(rightPath) << "\033[0m\n";
            return;
        }

        std::vector<std::string> leftLines;
        std::vector<std::string> rightLines;
        std::string error;
        if (!ReadTextFileLines(leftPath, leftLines, error)) {
            std::cout << "\033[1;31mError reading left file: " << error << "\033[0m\n";
            return;
        }
        if (!ReadTextFileLines(rightPath, rightLines, error)) {
            std::cout << "\033[1;31mError reading right file: " << error << "\033[0m\n";
            return;
        }

        size_t maxLines = (std::max)(leftLines.size(), rightLines.size());
        size_t diffCount = 0;
        for (size_t i = 0; i < maxLines; ++i) {
            const std::string left = (i < leftLines.size()) ? leftLines[i] : std::string();
            const std::string right = (i < rightLines.size()) ? rightLines[i] : std::string();
            if (left != right) {
                ++diffCount;
                std::cout << "Line " << (i + 1) << "\n";
                std::cout << "< " << left << "\n";
                std::cout << "> " << right << "\n";
            }
        }

        if (diffCount == 0) {
            std::cout << "\033[1;32mFiles are identical.\033[0m\n";
        } else {
            std::cout << "\033[1;33mDiff complete: " << diffCount << " differing line(s).\033[0m\n";
        }
    }

    static bool MovePathToRecycleBin(const std::wstring& path) {
        std::wstring from = path;
        from.push_back(L'\0'); // double-null terminated string list for SHFileOperation

        SHFILEOPSTRUCTW fileOp = {};
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = from.c_str();
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

        int result = SHFileOperationW(&fileOp);
        return result == 0 && !fileOp.fAnyOperationsAborted;
    }
};

