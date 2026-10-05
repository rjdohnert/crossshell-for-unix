#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>

std::string pathToUtf8(const fs::path& p) {
    const std::wstring& ws = p.native();
    if (ws.empty()) return {};
    int req = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (req <= 1) return {};
    std::string out(static_cast<size_t>(req) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), req, nullptr, nullptr);
    return out;
}

std::string filenameToUtf8(const fs::path& p) {
    std::wstring ws = p.filename().native();
    if (ws.empty()) ws = p.native();
    int req = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (req <= 1) return {};
    std::string out(static_cast<size_t>(req) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), req, nullptr, nullptr);
    return out;
}

std::string extensionToUtf8(const fs::path& p) {
    std::wstring ws = p.extension().native();
    if (ws.empty()) return {};
    int req = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (req <= 1) return {};
    std::string out(static_cast<size_t>(req) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), req, nullptr, nullptr);
    return out;
}

// ============================================================================
// ColorTheme
// ============================================================================

std::string ColorTheme::classify(const fs::path& path, DWORD attr, bool isLink) {
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        return DIR_BLUE;
    }
    if (isLink) {
        return LINK_BRIGHT;
    }

    std::string ext = extensionToUtf8(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { 
        return static_cast<char>(::tolower(c)); 
    });

    // Executables -> Cyan
    static const std::unordered_set<std::string> exes = {
        ".exe", ".bat", ".cmd", ".com", ".ps1", ".msi", ".scr", ".vbs"
    };
    if (exes.count(ext)) return EXE_CYAN;

    // Source Files -> Orange
    static const std::unordered_set<std::string> sources = {
        ".c", ".cpp", ".cxx", ".cc", ".h", ".hpp", ".cs", ".rs", ".go", 
        ".py", ".java", ".js", ".ts", ".html", ".css", ".sql", ".sh", 
        ".asm", ".json", ".xml", ".yaml", ".yml", ".toml", ".lua"
    };
    if (sources.count(ext)) return SRC_ORANGE;

    // Image Files -> Pink
    static const std::unordered_set<std::string> images = {
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg", ".webp", ".ico", 
        ".tiff", ".tif", ".psd", ".raw"
    };
    if (images.count(ext)) return IMG_PINK;

    // Archive Files -> Red
    static const std::unordered_set<std::string> archives = {
        ".zip", ".tar", ".gz", ".7z", ".rar", ".bz2", ".xz", ".cab", 
        ".iso", ".tgz", ".lz", ".zst"
    };
    if (archives.count(ext)) return ARC_RED;

    // Multimedia Files -> Purple
    static const std::unordered_set<std::string> media = {
        ".mp3", ".mp4", ".wav", ".mkv", ".avi", ".flac", ".mov", ".wmv", 
        ".ogg", ".m4a", ".aac", ".webm", ".wma", ".mpg", ".mpeg"
    };
    if (media.count(ext)) return MEDIA_PURPLE;

    return RESET;
}

// ============================================================================
// WindowsSecurityHelper
// ============================================================================

std::string WindowsSecurityHelper::toUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string out(static_cast<size_t>(required) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), required, nullptr, nullptr);
    return out;
}

WindowsSecurityHelper::SecurityInfo WindowsSecurityHelper::query(const fs::path& path) {
    SecurityInfo info;
    PSECURITY_DESCRIPTOR pSD = nullptr;
    PSID pOwnerSid = nullptr;
    PACL pDacl = nullptr;

    DWORD res = GetNamedSecurityInfoW(
        path.c_str(),
        SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &pOwnerSid,
        nullptr,
        &pDacl,
        nullptr,
        &pSD
    );

    if (res == ERROR_SUCCESS && pSD != nullptr) {
        if (pOwnerSid && IsValidSid(pOwnerSid)) {
            WCHAR name[256];
            WCHAR dom[256];
            DWORD nameLen = 256;
            DWORD domLen = 256;
            SID_NAME_USE use;

            if (LookupAccountSidW(nullptr, pOwnerSid, name, &nameLen, dom, &domLen, &use)) {
                std::wstring wsOwner(name);
                std::wstring wsDomain(dom);
                info.owner = toUtf8(wsOwner);
                info.domain = toUtf8(wsDomain);
            }
        }

        if (pDacl != nullptr && pDacl->AceCount > 0) {
            info.hasCustomAcl = true;
        }

        LocalFree(pSD);
    }
    return info;
}

// ============================================================================
// FileItem
// ============================================================================

FileItem::FileItem(const fs::path& p, bool fetchSecInfo) : path(p), name(filenameToUtf8(p)) {
    if (name.empty()) name = pathToUtf8(p);

    std::error_code ec;
    isSymlink = fs::is_symlink(p, ec);
    isDirectory = fs::is_directory(p, ec);

    attributes = GetFileAttributesW(p.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        attributes = 0;
    }

    if (!isDirectory && !isSymlink) {
        size = fs::file_size(p, ec);
        if (ec) size = 0;
    }

    lastWriteTime = fs::last_write_time(p, ec);

    if (fetchSecInfo) {
        secInfo = WindowsSecurityHelper::query(p);
    }
}

std::string FileItem::getWindowsModeString() const {
    std::string mode = "---------";
    if (attributes & FILE_ATTRIBUTE_DIRECTORY)     mode[0] = 'd';
    if (attributes & FILE_ATTRIBUTE_READONLY)      mode[1] = 'r';
    if (attributes & FILE_ATTRIBUTE_ARCHIVE)       mode[2] = 'a';
    if (attributes & FILE_ATTRIBUTE_HIDDEN)        mode[3] = 'h';
    if (attributes & FILE_ATTRIBUTE_SYSTEM)        mode[4] = 's';
    if (attributes & FILE_ATTRIBUTE_COMPRESSED)    mode[5] = 'c';
    if (attributes & FILE_ATTRIBUTE_ENCRYPTED)     mode[6] = 'e';
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) mode[7] = 'l';
    if (secInfo.hasCustomAcl)                      mode[8] = '+'; // HP-UX extended ACL marker
    return mode;
}

std::string FileItem::getFormattedTimestamp() const {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(
        lastWriteTime - fs::file_time_type::clock::now() + system_clock::now()
    );
    std::time_t tt = system_clock::to_time_t(sctp);
    std::tm tmVal;
    localtime_s(&tmVal, &tt);

    auto now = system_clock::to_time_t(system_clock::now());
    double diff = std::difftime(now, tt);

    char buf[32];
    if (diff > 15552000 || diff < -15552000) {
        std::strftime(buf, sizeof(buf), "%b %d  %Y", &tmVal);
    } else {
        std::strftime(buf, sizeof(buf), "%b %d %H:%M", &tmVal);
    }
    return std::string(buf);
}

// ============================================================================
// ListingEngine
// ============================================================================

ListingEngine::ListingEngine(ListingOptions opts) : options(std::move(opts)) {}

int ListingEngine::getTerminalWidth() const {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
}

void ListingEngine::sortItems(std::vector<FileItem>& items) const {
    std::sort(items.begin(), items.end(), [&](const FileItem& a, const FileItem& b) {
        if (options.sortByTime) {
            if (a.lastWriteTime != b.lastWriteTime)
                return options.reverseSort ? (a.lastWriteTime < b.lastWriteTime) 
                                           : (a.lastWriteTime > b.lastWriteTime);
        }
        if (options.sortBySize) {
            if (a.size != b.size)
                return options.reverseSort ? (a.size < b.size) : (a.size > b.size);
        }
        return options.reverseSort ? (a.name > b.name) : (a.name < b.name);
    });
}

void ListingEngine::listDirectory(const fs::path& dirPath, bool printHeader) {
    if (printHeader) {
        std::cout << "\n" << pathToUtf8(dirPath) << ":\n";
    }

    std::vector<FileItem> items;
    std::error_code ec;

    if (options.all) {
        items.emplace_back(dirPath / ".", options.longFormat);
        items.back().name = ".";
        items.emplace_back(dirPath / "..", options.longFormat);
        items.back().name = "..";
    }

    for (const auto& entry : fs::directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec)) {
        std::string fname = filenameToUtf8(entry.path());

        DWORD attr = GetFileAttributesW(entry.path().c_str());
        bool isHidden = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_HIDDEN);

        if (!options.all && !options.almostAll) {
            if (isHidden || (fname.length() > 0 && fname[0] == '.')) {
                continue;
            }
        }

        items.emplace_back(entry.path(), options.longFormat || !options.outputFormat.empty());
    }

    sortItems(items);

    if (!options.outputFormat.empty()) {
        LsReporter::printStructuredEntries(items, options.outputFormat);
        return;
    }

    if (options.longFormat) {
        LsReporter::printLong(items, options);
    } else if (options.singleColumn) {
        for (const auto& it : items) {
            std::cout << LsReporter::decorateName(it, options) << "\n";
        }
    } else {
        LsReporter::printColumns(items, options, getTerminalWidth());
    }

    if (options.recursive) {
        for (const auto& it : items) {
            if (it.isDirectory && it.name != "." && it.name != "..") {
                listDirectory(it.path, true);
            }
        }
    }
}

void ListingEngine::listSingleItem(const fs::path& p) {
    std::vector<FileItem> singleItem = { FileItem(p, options.longFormat) };
    if (options.longFormat) {
        std::cout << singleItem[0].getWindowsModeString() << " "
                  << singleItem[0].secInfo.domain << "\\" << singleItem[0].secInfo.owner << " "
                  << singleItem[0].size << " "
                  << singleItem[0].getFormattedTimestamp() << " "
                  << singleItem[0].name << "\n";
    } else {
        std::cout << singleItem[0].name << "\n";
    }
}

void ListingEngine::listStructuredTarget(const fs::path& p) {
    std::vector<FileItem> items;
    if (fs::is_directory(p)) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
            std::string fname = filenameToUtf8(entry.path());
            DWORD attr = GetFileAttributesW(entry.path().c_str());
            bool isHidden = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_HIDDEN);
            if (!options.all && !options.almostAll) {
                if (isHidden || (!fname.empty() && fname[0] == '.')) continue;
            }
            items.emplace_back(entry.path(), true);
        }
    } else {
        items.emplace_back(p, true);
    }
    std::sort(items.begin(), items.end(), [&](const FileItem& a, const FileItem& b) {
        return a.name < b.name;
    });
    LsReporter::printStructuredEntries(items, options.outputFormat);
}
