#include "cli_parser.hpp"
#include "file_target.hpp"
#include "storage_manager.hpp"
#include "sync_app.hpp"
#include "sync_help.hpp"
#include "sync_target.hpp"
#include "system_security.hpp"

int SyncApp::run(int argc, char* argv[]) {
        if (!CliParser::parse(argc, argv, options)) {
            return 2;
        }

        if (options.show_help) {
            std::cout << BSD_MANUAL;
            return 0;
        }

        if (options.show_version) {
            std::cout << "sync 2.0\n";
            return 0;
        }

        bool elevated = SystemSecurity::is_elevated();
        if (!elevated && !options.quiet) {
            std::cerr << "sync: warning: flushing disk volumes requires Administrator privileges.\n"
                      << "      If writes fail, re-run from an elevated console.\n";
        }

        // Flush standard runtime streams first (POSIX compatibility)
        _flushall();

        std::vector<std::unique_ptr<ISyncTarget>> targets;

        if (options.targets.empty()) {
            // Default BSD behavior: Synchronize all logical disk drives
            auto vols = StorageManager::discover_volumes(options.removable_only);
            for (auto& v : vols) {
                targets.push_back(std::move(v));
            }
        } else {
            // Synchronize specified files / paths
            for (const auto& path : options.targets) {
                targets.push_back(std::make_unique<FileTarget>(path, options.file_system));
            }
        }

        if (targets.empty()) {
            if (!options.quiet) {
                std::cout << "sync: no matching storage targets found.\n";
            }
            return 0;
        }

        int failed_count = 0;
        auto start_time = std::chrono::steady_clock::now();

        for (const auto& target : targets) {
            std::string err;
            bool ok = target->flush(err);

            if (!ok) {
                failed_count++;
                if (!options.quiet) {
                    std::cerr << "sync: " << target->get_name() << ": " << err << "\n";
                }
            } else if (options.verbose) {
                std::cout << "sync: " << std::left << std::setw(6) << target->get_name()
                          << " (" << target->get_description() << ") ... OK\n";
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        if (options.verbose) {
            std::cout << "sync: completed in " << elapsed << " ms ("
                      << (targets.size() - failed_count) << "/" << targets.size() 
                      << " targets committed to disk)\n";
        }

        return (failed_count == 0) ? 0 : 1;
    }
