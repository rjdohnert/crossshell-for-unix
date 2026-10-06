#ifndef MOUNT_OPTIONS_HPP
#define MOUNT_OPTIONS_HPP

#include "mount.hpp"

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
    static void ShowUsage();
    static void ShowVersion();
    bool Parse(int argc, wchar_t* argv[], MountOptions& opts) const;
};

#endif // MOUNT_OPTIONS_HPP
