class DskctlApplication {
public:
    static bool PromptYesNo(const std::string& prompt) {
        std::cout << prompt << " [y/N]: ";
        std::string response;
        std::getline(std::cin, response);
        if (response.empty()) {
            return false;
        }
        return StringHelper::ToLowerCopy(response) == "y" || StringHelper::ToLowerCopy(response) == "yes";
    }

    static bool PromptConfirmDelete(const std::string& prompt) {
        std::cout << prompt << " Type 'CONFIRM' to continue: ";
        std::string response;
        std::getline(std::cin, response);
        return StringHelper::ToLowerCopy(response) == "confirm";
    }

    int Run(int argc, char* argv[]) const {
        ConsoleEnvironment::EnableVirtualTerminal();

        if (!SecurityInspector::IsCurrentUserAdmin()) {
            std::cerr << RED << BOLD << " [WARNING] " << RESET
                      << "dskctl requires elevated Administrator privileges.\n"
                      << "Please run this executable from an elevated terminal session.\n";
            return 1;
        }

        DskctlOptions opts;
        if (!opts.Parse(argc, argv)) {
            return 1;
        }

        if (opts.showHelp) {
            DskctlReporter::PrintHelp(argv[0], opts.targets);
            return 0;
        }

        std::cout << BOLD << CYAN << "========================================\n";
        std::cout << "   dskctl - Volume & Disk Control       \n";
        std::cout << "========================================\n" << RESET;
        if (opts.config.dryRun) {
            std::cout << YELLOW << BOLD << "[DRY-RUN MODE] Previewing cleanup actions only. No files will be modified.\n" << RESET;
        } else {
            std::cout << YELLOW << BOLD << "[EXECUTE MODE] Cleanup actions will be performed.\n" << RESET;
        }

        DskctlLogger::AppendLogLine("dskctl session started");
        DskctlReporter::PrintPreviewReport(opts.targets, opts.config);

        if (!opts.config.force && !opts.config.dryRun) {
            std::cout << "This action may remove files from temporary and cache locations.\n";
            if (!PromptYesNo("Continue?")) {
                std::cout << "Operation cancelled.\n";
                DskctlLogger::AppendLogLine("Operation cancelled by user");
                return 0;
            }
            if (!PromptConfirmDelete("This action is destructive and cannot be undone.")) {
                std::cout << "Operation cancelled.\n";
                DskctlLogger::AppendLogLine("Operation cancelled by user");
                return 0;
            }
        }

        CleanSummary summary;

        for (const auto& target : opts.targets) {
            if (!target.selected) continue;

            std::cout << BOLD << "[*] Executing Target: " << target.name << RESET << "\n";
            DskctlLogger::AppendLogLine("Executing target: " + target.name);

            if (target.flag == "--recycle-bin") {
                DiskCleanerEngine::CleanRecycleBin(opts.config, summary.totalBytesFreed);
                std::cout << GREEN << "    -> Recycle Bin purge requested\n" << RESET;
                DskctlLogger::AppendLogLine("Recycle Bin purge requested");
                continue;
            }

            uint64_t categoryBytes = 0;
            size_t categoryFiles = 0;
            size_t categorySkipped = 0;
            size_t categoryErrors = 0;

            for (const auto& rawPath : target.rawPaths) {
                fs::path p = StringHelper::ExpandEnvPath(rawPath);
                if (p.empty()) {
                    categorySkipped++;
                    continue;
                }

                uint64_t pathBytes = 0;
                size_t pathFiles = 0;
                size_t pathSkipped = 0;
                size_t pathErrors = 0;

                DiskCleanerEngine::CleanDirectory(p, opts.config, pathBytes, pathFiles, pathSkipped, pathErrors);

                categoryBytes += pathBytes;
                categoryFiles += pathFiles;
                categorySkipped += pathSkipped;
                categoryErrors += pathErrors;

                std::cout << "    Path: " << p.string() << "\n";
                std::cout << GREEN << "    -> " << (opts.config.dryRun ? "Would reclaim" : "Reclaimed") << ": "
                          << StringHelper::FormatBytes(pathBytes) << " (" << pathFiles << " items, "
                          << pathSkipped << " skipped, " << pathErrors << " errors)\n" << RESET;
                DskctlLogger::AppendLogLine("Path: " + p.string() + " => " + std::to_string(pathFiles) + " items, " + std::to_string(pathSkipped) + " skipped, " + std::to_string(pathErrors) + " errors");
            }

            summary.totalBytesFreed += categoryBytes;
            summary.totalFilesRemoved += categoryFiles;
            summary.totalSkipped += categorySkipped;
            summary.totalErrors += categoryErrors;
        }

        DskctlReporter::PrintSummary(summary, opts.config);

        return summary.totalErrors == 0 ? 0 : 1;
    }
};
