#pragma once

#include "wipe.hpp"

class PathUtils {
private:
    static std::mt19937_64& GetRng();

public:
    static std::string RandomString(size_t length);

    static std::string NormalizePath(const std::string& path);

    static std::string JoinPath(const std::string& base, const std::string& child);

    static std::string GetParentDir(const std::string& path);

    static std::string GetRandomFilePath(const std::string& original_path);

    static std::mt19937_64& Rng();
};
