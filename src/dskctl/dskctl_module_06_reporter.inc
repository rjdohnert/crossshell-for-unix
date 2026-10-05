class DskctlReporter {
public:
    static void PrintHelp(const char* exeName, const std::vector<CleanTarget>& targets) {
        std::cout << R"(dskctl(1)                 CrossShell for UNIX Reference Manual                 dskctl(1)

    NAME
        dskctl - preview-first disk cleanup and cache maintenance utility

    SYNOPSIS
        dskctl [OPTIONS] [TARGET_FLAGS...]

    DESCRIPTION
        Provides a preview-first Windows cleanup utility for temporary and
        cache-related files. It can inspect and optionally remove data from
        known temporary locations, Windows update caches, crash dumps, log
        directories, and the Recycle Bin.

        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -a, --all
            Select all available disk cleanup targets.

        -s, --safe
            Select only safe, non-destructive cleanup targets.

        -n, --dry-run
            Preview cleanup actions without removing files (default).

        -x, --execute
            Perform the cleanup actions after previewing.

        -t, --trash
            Move items to the Recycle Bin instead of deleting permanently.

        -v, --verbose
            Output full paths of all evaluated files.

        -f, --force
            Bypass interactive confirmation prompt.

        --json
            Output summary formatted as JSON.

        --csv
            Output summary formatted as CSV.

        --table
            Output summary formatted as a table.

        --pipe COMMAND
            Send formatted summary through COMMAND pipeline.

        -h, --help
            Display this reference manual and exit.

    TARGETS
        --temp
            Cleans local user temp directory (%TEMP%).

        --system-temp
            Cleans Windows system temp directory (%WINDIR%\Temp).

        --update-cache
            Cleans Windows Update download packages.

        --error-dumps
            Cleans MEMORY.DMP and Minidump crash files.

        --logs
            Cleans Windows Setup and system log files.

        --recycle-bin
            Empties system-wide Recycle Bin.

        --wer
            Cleans WER crash dumps and report queues.

        --shaders
            Cleans GPU and DirectX shader caches.

        --delivery-opt
            Cleans Delivery Optimization cache files.

        --inet-cache
            Cleans INetCache temporary files.

    EXAMPLES
        dskctl -n
            Preview standard safe cleanup targets without deleting files.

        dskctl --temp --recycle-bin -x
            Clean temporary files and empty the Recycle Bin.

        dskctl -a -x -f
            Clean all targets without interactive confirmation.

    CrossShell for UNIX                                                    dskctl(1)
)";
    }

    static void PrintPreviewReport(const std::vector<CleanTarget>& targets, const DskctlConfig& config) {
        std::cout << "\nTargets Scheduled for Review:\n";
        DskctlLogger::AppendLogLine("Preview Report Start");

        for (const auto& target : targets) {
            if (!target.selected) continue;

            std::cout << "  [" << (target.destructive ? RED + std::string("DANGER") + RESET : GREEN + std::string("SAFE") + RESET)
                      << "] Target: " << target.name << " (Risk: " << target.risk << ")\n";
            DskctlLogger::AppendLogLine("  Target: " + target.name + " (" + target.risk + ")");

            if (target.flag == "--recycle-bin") {
                SHQUERYRBINFO rbInfo = { sizeof(SHQUERYRBINFO) };
                if (SUCCEEDED(SHQueryRecycleBinW(NULL, &rbInfo))) {
                    std::cout << "  * Recycle Bin Items: " << rbInfo.i64NumItems << " (" << StringHelper::FormatBytes(rbInfo.i64Size) << ")\n";
                    DskctlLogger::AppendLogLine("  * Recycle Bin items: " + std::to_string(rbInfo.i64NumItems));
                }
                continue;
            }

            for (const auto& rawPath : target.rawPaths) {
                fs::path p = StringHelper::ExpandEnvPath(rawPath);
                std::string resolvedText = p.string();
                std::error_code ec;
                if (!fs::exists(p, ec)) {
                    std::cout << "  * unresolved path: " << resolvedText << "\n";
                    DskctlLogger::AppendLogLine("  * unresolved path: " + resolvedText);
                    continue;
                }

                std::vector<std::string> items;
                size_t itemCount = 0;
                DiskCleanerEngine::CollectPreviewItems(p, items, itemCount);
                std::cout << "  * " << p.string() << " (" << itemCount << " items)\n";
                DskctlLogger::AppendLogLine("  * " + p.string() + " (" + std::to_string(itemCount) + " items)");

                size_t previewLimit = 5;
                for (size_t i = 0; i < std::min(previewLimit, items.size()); ++i) {
                    std::cout << "      - " << items[i] << "\n";
                    DskctlLogger::AppendLogLine("      - " + items[i]);
                }
                if (items.size() > previewLimit) {
                    std::cout << "      ... plus " << (items.size() - previewLimit) << " more\n";
                    DskctlLogger::AppendLogLine("      ... plus " + std::to_string(items.size() - previewLimit) + " more");
                }
            }
        }

        std::cout << "Mode: " << (config.dryRun ? "preview" : "execute") << "\n";
        DskctlLogger::AppendLogLine("Mode: " + std::string(config.dryRun ? "preview" : "execute"));
    }

    static void PrintSummary(const CleanSummary& summary, const DskctlConfig& config) {
        std::cout << BOLD << CYAN << "\n----------------------------------------\n";
        std::cout << "DSKCTL SUMMARY\n";
        std::cout << "----------------------------------------\n" << RESET;
        std::cout << "Total Space " << (config.dryRun ? "Reclaimable" : "Reclaimed") << ": "
                  << BOLD << GREEN << StringHelper::FormatBytes(summary.totalBytesFreed) << RESET << "\n";
        std::cout << "Total Objects Processed: " << summary.totalFilesRemoved << "\n";
        std::cout << "Skipped Entries: " << summary.totalSkipped << "\n";
        std::cout << "Errors: " << summary.totalErrors << "\n";
        std::cout << "Status: " << (summary.totalErrors == 0 ? (BOLD + std::string(GREEN) + "SUCCESS") : (BOLD + std::string(RED) + "COMPLETED WITH ERRORS")) << RESET << "\n";

        DskctlLogger::AppendLogLine("Summary: total reclaimed=" + std::to_string(summary.totalBytesFreed) + 
                                    ", processed=" + std::to_string(summary.totalFilesRemoved) + 
                                    ", skipped=" + std::to_string(summary.totalSkipped) + 
                                    ", errors=" + std::to_string(summary.totalErrors));

        if (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) {
            std::string text = (config.outputFormat == OutputFormat::Json) ?
                "{\"status\":\"" + std::string(summary.totalErrors == 0 ? "success" : "failure") + "\",\"bytes\":" + std::to_string(summary.totalBytesFreed) + ",\"objects\":" + std::to_string(summary.totalFilesRemoved) + ",\"skipped\":" + std::to_string(summary.totalSkipped) + ",\"errors\":" + std::to_string(summary.totalErrors) + "}\n" :
                (config.outputFormat == OutputFormat::Csv) ?
                "status,bytes,objects,skipped,errors\n" + std::string(summary.totalErrors == 0 ? "success," : "failure,") + std::to_string(summary.totalBytesFreed) + "," + std::to_string(summary.totalFilesRemoved) + "," + std::to_string(summary.totalSkipped) + "," + std::to_string(summary.totalErrors) + "\n" :
                "STATUS\tBYTES\tOBJECTS\tSKIPPED\tERRORS\n" + std::string(summary.totalErrors == 0 ? "success\t" : "failure\t") + std::to_string(summary.totalBytesFreed) + "\t" + std::to_string(summary.totalFilesRemoved) + "\t" + std::to_string(summary.totalSkipped) + "\t" + std::to_string(summary.totalErrors) + "\n";

            if (!config.pipeCommand.empty()) {
                FILE* pipe = _popen(config.pipeCommand.c_str(), "w");
                if (pipe) {
                    fwrite(text.data(), 1, text.size(), pipe);
                    _pclose(pipe);
                }
            } else {
                std::cout << text;
            }
        }
    }
};
