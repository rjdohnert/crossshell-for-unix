#pragma once

#include "dependency.hpp"
#include "file_record.hpp"
#include "rpm.hpp"

struct RpmMetadata {
    std::string name;
    std::string version;
    std::string release;
    std::string epoch = "0";
    std::string summary;
    std::string description;
    std::string vendor;
    std::string license;
    std::string group;
    std::string url;
    std::string os;
    std::string arch;
    std::string payloadCompressor = "gzip";
    std::string headerSha256;
    uint32_t size = 0;
    uint32_t buildTime = 0;
    std::string preInScript;
    std::string postInScript;
    std::string preUnScript;
    std::string postUnScript;
    std::vector<FileRecord> files;
    std::vector<Dependency> requirements;
    std::vector<Dependency> provides;
};
