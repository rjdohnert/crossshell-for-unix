#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct GzipMember {
    uint64_t offset = 0;
    uint64_t length = 0;
};

bool FindGzipMembers(const std::filesystem::path& path,
                     std::vector<GzipMember>& members, std::wstring& error);
