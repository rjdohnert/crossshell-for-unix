#ifndef LS_HPP
#define LS_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <unordered_set>
#include <algorithm>

#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

std::string pathToUtf8(const fs::path& p);
std::string filenameToUtf8(const fs::path& p);
std::string extensionToUtf8(const fs::path& p);

class ColorTheme {
public:
    inline static const std::string RESET        = "\033[0m";
    inline static const std::string BOLD         = "\033[1m";
    inline static const std::string DIR_BLUE     = "\033[38;2;68;142;255m";  // Directories: Blue
    inline static const std::string EXE_CYAN     = "\033[38;2;0;235;255m";   // .exe/bin: Cyan
    inline static const std::string SRC_ORANGE   = "\033[38;2;255;140;0m";   // Sources: Orange
    inline static const std::string IMG_PINK     = "\033[38;2;255;105;180m"; // Images: Pink
    inline static const std::string ARC_RED      = "\033[38;2;255;70;70m";   // Archives: Red
    inline static const std::string MEDIA_PURPLE = "\033[38;2;186;85;211m";  // Multimedia: Purple
    inline static const std::string LINK_BRIGHT  = "\033[38;2;120;255;120m"; // Reparse/Links: Bright Green
    inline static const std::string ATTR_COLOR   = "\033[38;2;170;170;170m";

    static std::string classify(const fs::path& path, DWORD attr, bool isLink);
};

class WindowsSecurityHelper {
public:
    struct SecurityInfo {
        std::string owner = "-";
        std::string domain = "-";
        bool hasCustomAcl = false;
    };

    static std::string toUtf8(const std::wstring& value);
    static SecurityInfo query(const fs::path& path);
};

class FileItem {
public:
    fs::path path;
    std::string name;
    DWORD attributes = 0;
    uint64_t size = 0;
    fs::file_time_type lastWriteTime;
    bool isSymlink = false;
    bool isDirectory = false;
    WindowsSecurityHelper::SecurityInfo secInfo;

    FileItem(const fs::path& p, bool fetchSecInfo);

    std::string getWindowsModeString() const;
    std::string getFormattedTimestamp() const;
};

#endif // LS_HPP
