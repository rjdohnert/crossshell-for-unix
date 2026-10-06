#pragma once

#include "purge.hpp"
#include "options.hpp"

class SecureEraser {
public:
    static bool EraseFile(const fs::path& filePath, uint64_t fileSize);
};

class FileVersionExtractor {
public:
    static FileRecord Parse(const fs::directory_entry& entry);
};

class PurgeEngine {
public:
    static int Execute(const PurgeOptions& opts, std::ostream& outStream);
};
