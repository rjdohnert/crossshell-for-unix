#pragma once

#include "rpmbuild.hpp"

class HeaderSerializer {
private:
    struct TagEntry {
        uint32_t tag;
        uint32_t type;
        uint32_t count;
        std::vector<char> data;
    };
    std::vector<TagEntry> entries;

    static size_t getAlignment(uint32_t type);

public:
    void addString(uint32_t tag, const std::string& str);

    void addInt32(uint32_t tag, uint32_t val);

    void addUint16Array(uint32_t tag, const std::vector<uint16_t>& arr);

    void addUint32Array(uint32_t tag, const std::vector<uint32_t>& arr);

    void addStringArray(uint32_t tag, const std::vector<std::string>& arr);

    std::vector<char> serialize();
};
