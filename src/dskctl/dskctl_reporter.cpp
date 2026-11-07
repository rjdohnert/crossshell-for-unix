/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "dskctl_reporter.hpp"

#include "dskctl_helpers.hpp"

#include <iostream>
#include <string>
#include <string_view>

namespace {

std::string EscapeJson(std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string output;
    output.reserve(value.size());
    for (unsigned char character : value) {
        switch (character) {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (character < 0x20) {
                    output += "\\u00";
                    output += hex[character >> 4];
                    output += hex[character & 0x0f];
                } else {
                    output += static_cast<char>(character);
                }
        }
    }
    return output;
}

std::string EscapeCsv(std::string_view value) {
    std::string output;
    output.reserve(value.size() + 3);
    output += '"';
    std::size_t firstVisible = value.find_first_not_of(" \t\r\n\v\f");
    if (firstVisible != std::string_view::npos &&
        (value[firstVisible] == '=' || value[firstVisible] == '+' ||
         value[firstVisible] == '-' || value[firstVisible] == '@')) {
        output += '\'';
    }
    for (char character : value) {
        if (character == '"') output += "\"\"";
        else output += character;
    }
    output += '"';
    return output;
}

std::string JoinIssues(const TargetReport& report) {
    std::string output;
    for (const std::string& issue : report.issues) {
        if (!output.empty()) output += "; ";
        output += issue;
    }
    return output;
}

std::string EscapeTsv(std::string value) {
    for (char& character : value) {
        if (character == '\t' || character == '\r' || character == '\n') {
            character = ' ';
        }
    }
    return value;
}

void PrintTargetDetails(const TargetReport& report) {
    std::cout << "  " << report.targetName << ": "
              << report.files << " files, " << FormatBytes(report.bytes)
              << ", " << report.skipped << " skipped, "
              << report.errors << " errors\n";
    for (const std::string& issue : report.issues) {
        std::cout << "    ! " << issue << '\n';
    }
}

} // namespace

void PrintHelp(const std::vector<CleanupTarget>& targets) {
    std::cout << R"(dskctl(1)                 CrossShell for UNIX Reference Manual                 dskctl(1)

    NAME
        dskctl - preview and clean approved Windows temporary/cache locations

    SYNOPSIS
        dskctl [OPTIONS] [TARGETS...]

    OPTIONS
        -n, --dry-run
            Preview cleanup without modifying files (default).
        -x, --execute
            Perform cleanup after previewing and confirming.
        -s, --safe
            Select all low- and medium-risk targets only.
        -a, --all
            Select all targets, including high-risk targets.
        -t, --trash
            Move items to the Recycle Bin instead of permanently deleting them.
        -v, --verbose
            Show up to five sample paths per target.
        -f, --force
            Bypass execution prompts. Use only with a deliberate target selection.
        --json, --csv, --table
            Emit clean machine-readable output (one format only).
        -h, --help
            Display this reference manual and exit.

    TARGETS
)";
    for (const CleanupTarget& target : targets) {
        std::cout << "        " << target.flag << "\n"
                  << "            " << target.description << "\n";
    }
    std::cout << R"(
    NOTES
        Preview output never follows or removes reparse points (symbolic links,
        junctions, or mount points). Execution is limited to fixed Windows and
        Known Folder locations; environment-variable paths are not trusted.

    CrossShell for UNIX                                                    dskctl(1)
)";
}

void PrintHumanPreview(const std::vector<TargetReport>& previews,
                       bool execute, bool useRecycleBin, bool verbose) {
    std::cout << "dskctl - approved cleanup preview\n"
              << (execute ? "Mode: execute after confirmation\n"
                          : "Mode: dry run; no files will be modified\n");
    if (execute && useRecycleBin) {
        std::cout << "Removal method: Recycle Bin\n";
    } else if (execute) {
        std::cout << "Removal method: permanent deletion\n";
    }

    for (const TargetReport& report : previews) {
        std::cout << "\n" << report.targetName << ": "
                  << report.files << " files, " << FormatBytes(report.bytes)
                  << ", " << report.skipped << " skipped, "
                  << report.errors << " errors\n";
        if (verbose) {
            for (const std::string& path : report.samplePaths) {
                std::cout << "  - " << path << '\n';
            }
            if (report.files > report.samplePaths.size()) {
                std::cout << "  ... more files omitted\n";
            }
        }
        for (const std::string& issue : report.issues) {
            std::cout << "  ! " << issue << '\n';
        }
    }
}

void PrintHumanSummary(const CleanSummary& summary,
                       const DskctlOptions& options,
                       const std::filesystem::path& logPath,
                       bool logSaved) {
    std::cout << "\nDSKCTL SUMMARY\n"
              << "Mode: " << (summary.cancelled ? "cancelled" :
                              (options.dryRun ? "preview" : "execute")) << '\n'
              << ((options.dryRun || summary.cancelled)
                      ? "Estimated reclaimable: " : "Removed: ")
              << FormatBytes(summary.totalBytes) << '\n'
              << "Files: " << summary.totalFiles << '\n'
              << "Skipped: " << summary.totalSkipped << '\n'
              << "Errors: " << summary.totalErrors << '\n';
    if (logSaved) {
        std::cout << "Log: " << PathToUtf8(logPath) << '\n';
    } else {
        std::cout << "Log: unavailable or incomplete\n";
    }
    if (summary.cancelled) std::cout << "Status: cancelled; no cleanup was performed\n";
    for (const TargetReport& report : summary.targets) PrintTargetDetails(report);
}

void PrintStructuredSummary(const CleanSummary& summary,
                            const DskctlOptions& options,
                            const std::filesystem::path& logPath,
                            bool logSaved) {
    const std::string mode = summary.cancelled
        ? "cancelled" : (options.dryRun ? "preview" : "execute");
    const std::string log = logSaved ? PathToUtf8(logPath) : std::string();
    if (options.outputFormat == OutputFormat::Json) {
        std::cout << "{\"mode\":\"" << mode << "\",\"bytes\":"
                  << summary.totalBytes << ",\"files\":" << summary.totalFiles
                  << ",\"skipped\":" << summary.totalSkipped
                  << ",\"errors\":" << summary.totalErrors
                  << ",\"cancelled\":" << (summary.cancelled ? "true" : "false")
                  << ",\"logSaved\":" << (logSaved ? "true" : "false")
                  << ",\"logPath\":\"" << EscapeJson(log) << "\",\"targets\":[";
        for (std::size_t index = 0; index < summary.targets.size(); ++index) {
            const TargetReport& report = summary.targets[index];
            if (index != 0) std::cout << ',';
            std::cout << "{\"name\":\"" << EscapeJson(report.targetName)
                      << "\",\"bytes\":" << report.bytes
                      << ",\"files\":" << report.files
                      << ",\"skipped\":" << report.skipped
                      << ",\"errors\":" << report.errors << ",\"issues\":[";
            for (std::size_t issueIndex = 0;
                 issueIndex < report.issues.size(); ++issueIndex) {
                if (issueIndex != 0) std::cout << ',';
                std::cout << '"' << EscapeJson(report.issues[issueIndex]) << '"';
            }
            std::cout << "]}";
        }
        std::cout << "]}\n";
        return;
    }

    if (options.outputFormat == OutputFormat::Csv) {
        std::cout << "Target,Mode,Bytes,Files,Skipped,Errors,Issues,Cancelled,LogSaved\n";
        for (const TargetReport& report : summary.targets) {
            std::cout << EscapeCsv(report.targetName) << ','
                      << mode << ',' << report.bytes << ',' << report.files << ','
                      << report.skipped << ',' << report.errors << ','
                      << EscapeCsv(JoinIssues(report)) << ','
                      << (summary.cancelled ? "true" : "false") << ','
                      << (logSaved ? "true" : "false") << '\n';
        }
        std::cout << EscapeCsv("TOTAL") << ',' << mode << ','
                  << summary.totalBytes << ',' << summary.totalFiles << ','
                  << summary.totalSkipped << ',' << summary.totalErrors << ','
                  << EscapeCsv("") << ','
                  << (summary.cancelled ? "true" : "false") << ','
                  << (logSaved ? "true" : "false") << '\n';
        return;
    }

    std::cout << "Target\tMode\tBytes\tFiles\tSkipped\tErrors\tIssues\tCancelled\tLogSaved\n";
    for (const TargetReport& report : summary.targets) {
        std::cout << report.targetName << '\t' << mode << '\t' << report.bytes
                  << '\t' << report.files << '\t' << report.skipped << '\t'
                  << report.errors << '\t' << EscapeTsv(JoinIssues(report))
                  << '\t' << (summary.cancelled ? "true" : "false")
                  << '\t' << (logSaved ? "true" : "false")
                  << '\n';
    }
    std::cout << "TOTAL\t" << mode << '\t' << summary.totalBytes << '\t'
              << summary.totalFiles << '\t' << summary.totalSkipped << '\t'
              << summary.totalErrors << '\t' << '\t'
              << (summary.cancelled ? "true" : "false") << '\t'
              << (logSaved ? "true" : "false")
              << '\n';
}
