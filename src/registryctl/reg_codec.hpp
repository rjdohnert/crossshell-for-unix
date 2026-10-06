#pragma once

#include "registryctl.hpp"

class RegCodec {
public:
    static HKEY ParseRootKey(const std::string& keyStr);

    static bool SplitPath(const std::string& fullPath, HKEY& outRoot, std::string& outRootStr, std::string& outSubKey);

    static DWORD ParseTypeString(const std::string& typeStr);

    static bool EncodeData(DWORD type, const std::string& input, std::vector<uint8_t>& outBytes, std::string& err);
};
