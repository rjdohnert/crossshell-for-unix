#ifndef DSKCTL_MODELS_HPP
#define DSKCTL_MODELS_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class RiskLevel {
    Low,
    Medium,
    High
};

enum class OutputFormat {
    Human,
    Json,
    Csv,
    Table
};

struct CleanupTarget {
    std::string id;
    std::string flag;
    std::string name;
    std::string description;
    std::vector<std::filesystem::path> paths;
    RiskLevel risk = RiskLevel::Low;
    bool recycleBin = false;
    bool selected = false;
};

struct DskctlOptions {
    bool dryRun = true;
    bool useRecycleBin = false;
    bool verbose = false;
    bool force = false;
    bool safeOnly = false;
    bool showHelp = false;
    OutputFormat outputFormat = OutputFormat::Human;
    std::vector<CleanupTarget> targets;
};

struct TargetReport {
    std::string targetName;
    std::uint64_t bytes = 0;
    std::size_t files = 0;
    std::size_t skipped = 0;
    std::size_t errors = 0;
    std::vector<std::string> samplePaths;
    std::vector<std::string> issues;
};

struct CleanSummary {
    std::vector<TargetReport> targets;
    std::uint64_t totalBytes = 0;
    std::size_t totalFiles = 0;
    std::size_t totalSkipped = 0;
    std::size_t totalErrors = 0;
    bool cancelled = false;
};

struct ParseResult {
    bool success = false;
    std::string error;
    DskctlOptions options;
};

#endif
