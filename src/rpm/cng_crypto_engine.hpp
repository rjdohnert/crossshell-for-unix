#pragma once

#include "rpm.hpp"

class CngCryptoEngine {
public:
    static std::string calculateFileSha256(const fs::path& filePath);
};
