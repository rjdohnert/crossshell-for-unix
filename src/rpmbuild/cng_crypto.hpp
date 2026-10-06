#pragma once

#include "rpmbuild.hpp"

class CngCrypto {
public:
    static std::string calculateFileSha256(const fs::path& filePath);

    static std::string calculateMemorySha256(const char* data, size_t len);
};
