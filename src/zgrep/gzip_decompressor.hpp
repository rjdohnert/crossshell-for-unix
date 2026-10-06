#pragma once

#include "process_runner.hpp"
#include "zgrep.hpp"

class GzipDecompressor {
private:
    ProcessRunner m_runner;

public:
    explicit GzipDecompressor(const ProcessRunner& runner);

    bool Decompress(const fs::path& inputPath, const fs::path& outputPath) const;
};
