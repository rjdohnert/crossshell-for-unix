#ifndef DU_MODELS_HPP
#define DU_MODELS_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace du {

enum class SizeFormatMode {
    Kilobytes,
    Megabytes,
    Bytes,
    HumanReadable
};

enum class OutputFormat {
    Human,
    Json,
    Csv,
    Table
};

struct Options {
    std::vector<std::filesystem::path> paths;
    std::vector<std::string> excludePatterns;
    std::string files0From;
    std::string pipeCommand;
    std::optional<std::size_t> maxDepth;
    std::size_t blockSize = 1024;
    SizeFormatMode sizeFormat = SizeFormatMode::Kilobytes;
    OutputFormat outputFormat = OutputFormat::Human;
    bool allFiles = false;
    bool summarizeOnly = false;
    bool total = false;
    bool apparentSize = false;
    bool nullTerminated = false;
    bool dereference = false;
    bool showHelp = false;
    bool showVersion = false;
};

struct UsageEntry {
    std::filesystem::path path;
    std::uint64_t bytes = 0;
};

struct ScanSummary {
    std::uint64_t totalBytes = 0;
    std::size_t fileCount = 0;
    std::size_t skippedCount = 0;
    std::size_t errorCount = 0;
};

struct ParseResult {
    bool success = false;
    std::string error;
    Options options;
};

} // namespace du

#endif
