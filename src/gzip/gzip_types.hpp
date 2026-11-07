#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class ReportFormat {
    none,
    json,
    csv,
    table
};

struct GzipOptions {
    bool keep = false;
    bool force = false;
    bool decompress = false;
    bool stdoutMode = false;
    bool showHelp = false;
    bool showVersion = false;
    ReportFormat reportFormat = ReportFormat::none;
    std::wstring pipeCommand;
    std::vector<std::filesystem::path> files;
};

struct GzipOperation {
    std::filesystem::path source;
    std::filesystem::path target;
};
