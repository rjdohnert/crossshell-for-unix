#pragma once

#include "process_runner.hpp"
#include "zcat.hpp"

class GzipStreamer {
private:
    ProcessRunner m_runner;

public:
    explicit GzipStreamer(const ProcessRunner& runner);

    int EmitGzipFile(const fs::path& inputPath, OutputFormat format) const;
};
