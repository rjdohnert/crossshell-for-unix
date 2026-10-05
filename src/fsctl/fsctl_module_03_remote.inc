// ============================================================================
// 3. REMOTE PROTOCOL ENGINES
// ============================================================================

class RemoteProtocolHelper {
public:
    static bool IsRemotePath(const std::wstring& path) {
        if (path.empty()) return false;
        std::wstring lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return std::towlower(c); });
        return lower.rfind(L"ftp://", 0) == 0 || lower.rfind(L"ssh://", 0) == 0;
    }

    static std::wstring BuildRemoteUri(const RemoteLocation& location) {
        std::wstring uri = location.scheme + L"://";
        if (!location.user.empty()) {
            uri += location.user;
            if (!location.password.empty()) {
                uri += L":" + location.password;
            }
            uri += L"@";
        }
        uri += location.host;
        if (location.path.empty()) {
            uri += L"/";
        } else {
            uri += location.path;
        }
        return uri;
    }

    static bool ParseRemoteLocation(const std::wstring& uri, RemoteLocation& location) {
        location = {};
        if (!IsRemotePath(uri)) return false;

        std::wstring lower = uri;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return std::towlower(c); });

        if (lower.rfind(L"ftp://", 0) == 0) {
            location.scheme = L"ftp";
        } else if (lower.rfind(L"ssh://", 0) == 0) {
            location.scheme = L"ssh";
        } else {
            return false;
        }

        std::wstring rest = uri.substr(location.scheme.size() + 3);
        size_t slash = rest.find(L'/');
        std::wstring authority = slash == std::wstring::npos ? rest : rest.substr(0, slash);
        std::wstring path = slash == std::wstring::npos ? L"/" : rest.substr(slash);
        if (path.empty()) path = L"/";

        size_t at = authority.rfind(L'@');
        if (at != std::wstring::npos) {
            std::wstring userinfo = authority.substr(0, at);
            location.host = authority.substr(at + 1);
            size_t colon = userinfo.find(L':');
            if (colon != std::wstring::npos) {
                location.user = userinfo.substr(0, colon);
                location.password = userinfo.substr(colon + 1);
            } else {
                location.user = userinfo;
            }
        } else {
            location.host = authority;
        }

        location.path = path;
        if (location.path.empty()) location.path = L"/";
        return true;
    }

    static std::wstring JoinRemotePath(const std::wstring& base, const std::wstring& child) {
        if (child.empty() || child == L".") return base;
        if (IsRemotePath(child)) return child;

        if (child == L"..") {
            RemoteLocation location;
            if (!ParseRemoteLocation(base, location)) return base;
            if (location.path.empty() || location.path == L"/") return base;
            size_t lastSlash = location.path.find_last_of(L"/");
            if (lastSlash == std::wstring::npos || lastSlash == 0) {
                location.path = L"/";
            } else {
                location.path = location.path.substr(0, lastSlash);
            }
            return BuildRemoteUri(location);
        }

        if (base.empty()) return child;
        if (base.back() == L'/' || base.back() == L'\\') return base + child;
        return base + L"/" + child;
    }

    static std::wstring ResolvePathForContext(const std::wstring& currentPath, const std::wstring& input) {
        if (input.empty()) return currentPath;
        if (IsRemotePath(input)) return input;
        if (IsRemotePath(currentPath)) return JoinRemotePath(currentPath, input);
        return PathHelper::NormalizePath(currentPath, input);
    }
};

class FtpEngine {
public:
    static bool ListDirectory(const RemoteLocation& location, std::vector<std::wstring>& entries) {
        ScopedInternetHandle hInternet(InternetOpenW(L"fsctl", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
        if (!hInternet.IsValid()) return false;

        ScopedInternetHandle hConnect(InternetConnectW(hInternet.Get(), location.host.c_str(), INTERNET_DEFAULT_FTP_PORT,
                                                      location.user.empty() ? L"anonymous" : location.user.c_str(),
                                                      location.password.empty() ? L"anonymous@" : location.password.c_str(),
                                                      INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0));
        if (!hConnect.IsValid()) return false;

        if (!FtpSetCurrentDirectoryW(hConnect.Get(), location.path.c_str())) return false;

        WIN32_FIND_DATAW findData;
        ScopedInternetHandle hFind(FtpFindFirstFileW(hConnect.Get(), L"*", &findData, 0, 0));
        if (!hFind.IsValid()) return false;

        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            entries.push_back(findData.cFileName);
        } while (InternetFindNextFileW(hFind.Get(), &findData));

        return true;
    }

    static std::string ReadFileContent(const RemoteLocation& location) {
        ScopedInternetHandle hInternet(InternetOpenW(L"fsctl", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
        if (!hInternet.IsValid()) return {};

        ScopedInternetHandle hConnect(InternetConnectW(hInternet.Get(), location.host.c_str(), INTERNET_DEFAULT_FTP_PORT,
                                                      location.user.empty() ? L"anonymous" : location.user.c_str(),
                                                      location.password.empty() ? L"anonymous@" : location.password.c_str(),
                                                      INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0));
        if (!hConnect.IsValid()) return {};

        ScopedInternetHandle hFile(FtpOpenFileW(hConnect.Get(), location.path.c_str(), GENERIC_READ, FTP_TRANSFER_TYPE_BINARY, 0));
        if (!hFile.IsValid()) return {};

        std::string output;
        char buffer[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hFile.Get(), buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            output.append(buffer, bytesRead);
        }
        return output;
    }
};

class SshEngine {
public:
    static std::wstring RunCommand(const RemoteLocation& location, const std::wstring& command) {
        std::wstring exePath;
        wchar_t resolved[MAX_PATH] = {0};

        DWORD length = SearchPathW(nullptr, L"ssh.exe", nullptr, MAX_PATH, resolved, nullptr);
        if (length > 0 && length < MAX_PATH) {
            exePath = resolved;
        } else {
            length = SearchPathW(nullptr, L"plink.exe", nullptr, MAX_PATH, resolved, nullptr);
            if (length > 0 && length < MAX_PATH) {
                exePath = resolved;
            }
        }

        if (exePath.empty()) {
            return L"SSH support requires OpenSSH (ssh.exe) or PuTTY (plink.exe) to be installed.";
        }

        std::wstring target = location.host;
        if (!location.user.empty()) {
            target = location.user + L"@" + location.host;
        }

        std::wstring cmdLine = L"\"" + exePath + L"\" -o BatchMode=yes \"" + target + L"\" \"" + command + L"\"";
        FILE* pipe = _wpopen(cmdLine.c_str(), L"r");
        if (!pipe) {
            return L"Unable to launch SSH client.";
        }

        wchar_t buffer[4096];
        std::wstring output;
        while (fgetws(buffer, _countof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        _pclose(pipe);
        return output;
    }
};

class RemoteInspector {
public:
    static void ListRemoteDirectory(const std::wstring& path) {
        RemoteLocation location;
        if (!RemoteProtocolHelper::ParseRemoteLocation(path, location)) {
            std::cout << "\033[1;31mUnsupported remote location.\033[0m\n";
            return;
        }

        std::cout << "\nContents of \033[1;36m" << StringHelper::ToNarrow(path) << "\033[0m:\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(12) << "Mode"
                  << std::setw(8) << "Type"
                  << std::setw(44) << "Name"
                  << std::right << std::setw(16) << "Size\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        if (location.scheme == L"ftp") {
            std::vector<std::wstring> entries;
            if (!FtpEngine::ListDirectory(location, entries)) {
                std::cout << "\033[1;31mError reading FTP directory.\033[0m\n";
                std::cout << "--------------------------------------------------------------------------------\n";
                return;
            }

            for (const auto& entry : entries) {
                std::string entryNarrow = StringHelper::ToNarrow(entry);
                int entryPad = 44 - static_cast<int>(entry.length());
                if (entryPad < 1) entryPad = 1;
                std::string padSpaces(static_cast<size_t>(entryPad), ' ');

                std::cout << std::left << std::setw(12) << "---------"
                          << std::setw(8) << "FILE"
                          << entryNarrow << padSpaces
                          << std::right << std::setw(16) << "-\n";
            }
            std::cout << "--------------------------------------------------------------------------------\n";
            return;
        }

        std::wstring output = SshEngine::RunCommand(location, L"ls -la " + location.path);
        if (output.empty()) {
            std::cout << "  <No output returned>\n";
        } else {
            std::cout << StringHelper::ToNarrow(output) << "\n";
        }
        std::cout << "--------------------------------------------------------------------------------\n";
    }

    static void CatRemoteFile(const std::wstring& path) {
        RemoteLocation location;
        if (!RemoteProtocolHelper::ParseRemoteLocation(path, location)) {
            std::cout << "\033[1;31mUnsupported remote location.\033[0m\n";
            return;
        }

        if (location.scheme == L"ftp") {
            std::string content = FtpEngine::ReadFileContent(location);
            if (content.empty()) {
                std::cout << "\033[1;31mError reading FTP file.\033[0m\n";
                return;
            }
            std::cout << content << "\n";
            return;
        }

        std::wstring output = SshEngine::RunCommand(location, L"cat " + location.path);
        std::cout << StringHelper::ToNarrow(output) << "\n";
    }
};

