#include "command_line_parser.hpp"
#include "delete_options.hpp"
#include "input_pipeline.hpp"
#include "userdel_app.hpp"
#include "windows_user_manager.hpp"

int Application::Run(int argc, char* argv[]) {
        DeleteOptions options;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                CommandLineParser::PrintHelp();
                return 0;
            } else if (arg == "--version") {
                CommandLineParser::PrintVersion();
                return 0;
            } else if (arg == "-v" || arg == "--verbose") {
                options.verbose = true;
            } else if (arg == "-f" || arg == "--force") {
                options.force = true;
            } else if (arg == "-r" || arg == "--remove") {
                options.removeHomeAndProfile = true;
            } else if (arg == "--pipe") {
                options.pipeMode = true;
            } else if (arg == "-R" || arg == "--root") {
                if (++i < argc) options.rootDir = argv[i];
            } else if (arg == "-Z" || arg == "--selinux-user") {
                // POSIX compatibility stub; do nothing
            } else if (!arg.empty() && arg[0] != '-') {
                options.username = arg;
            } else {
                std::cerr << "userdel: unrecognized option '" << arg << "'\n";
                std::cerr << "Try 'userdel --help' for more information.\n";
                return 2;
            }
        }

        // Automatic pipe detection or explicit --pipe flag
        if (options.pipeMode || (options.username.empty() && InputPipeline::IsPipeActive())) {
            return InputPipeline::ProcessBatchPipe(options);
        }

        if (options.username.empty()) {
            std::cerr << "userdel: error: no username specified. Use --help for usage.\n";
            return 2;
        }

        if (options.verbose) {
            std::cout << "Target User: " << options.username << "\n";
            std::cout << "Remove Profile/Home: " << (options.removeHomeAndProfile ? "Yes" : "No") << "\n";
            std::cout << "Force Kill Processes: " << (options.force ? "Yes" : "No") << "\n";
        }

        std::string err;
        if (!WindowsUserManager::DeleteAccount(options, err)) {
            std::cerr << "userdel: " << err << "\n";
            return 1;
        }

        if (options.verbose) {
            std::cout << "userdel: user '" << options.username << "' deleted successfully.\n";
        }

        return 0;
    }
