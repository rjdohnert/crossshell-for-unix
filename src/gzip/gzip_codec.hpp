#pragma once

#include <filesystem>
#include <string>

class GzipCodec {
public:
    static bool run(const std::filesystem::path& source,
                    const std::filesystem::path& temporaryOutput,
                    bool compress, std::wstring& error);
};
