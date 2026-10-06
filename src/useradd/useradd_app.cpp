#include "command_line_parser.hpp"
#include "input_pipeline.hpp"
#include "string_utils.hpp"
#include "user_account.hpp"
#include "useradd_app.hpp"
#include "windows_user_manager.hpp"

int Application::Run(int argc, char* argv[]) {
        if (argc < 2) {
            if (InputPipeline::IsPipeActive()) {
                InputPipeline::ProcessBatchPipe(false);
                return 0;
            }
            CommandLineParser::PrintHelp();
            return 1;
        }

        UserAccount user;
        bool verbose = false;
        bool pipeMode = false;
        bool stdinPassword = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                CommandLineParser::PrintHelp();
                return 0;
            } else if (arg == "--version") {
                CommandLineParser::PrintVersion();
                return 0;
            } else if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else if (arg == "--pipe") {
                pipeMode = true;
            } else if (arg == "--stdin-password") {
                stdinPassword = true;
            } else if (arg == "-c" || arg == "--comment") {
                if (++i < argc) user.comment = argv[i];
            } else if (arg == "-d" || arg == "--home-dir") {
                if (++i < argc) user.homeDir = argv[i];
            } else if (arg == "-g" || arg == "--gid" || arg == "--group") {
                if (++i < argc) user.primaryGroup = argv[i];
            } else if (arg == "-G" || arg == "--groups") {
                if (++i < argc) user.supplementaryGroups = StringUtils::Split(argv[i], ',');
            } else if (arg == "-p" || arg == "--password") {
                if (++i < argc) user.password = argv[i];
            } else if (arg == "-m" || arg == "--create-home") {
                user.createHomeDir = true;
            } else if (arg == "-M" || arg == "--no-create-home") {
                user.createHomeDir = false;
            } else if (arg == "-r" || arg == "--system") {
                user.isSystemAccount = true;
            } else if (arg == "-f" || arg == "--force-password-change") {
                user.mustChangePassword = true;
            } else if (arg == "--never-expires") {
                user.passwordNeverExpires = true;
            } else if (arg == "--disabled") {
                user.accountDisabled = true;
            } else if (!arg.empty() && arg[0] != '-') {
                user.username = arg;
            }
        }

        if (pipeMode) {
            InputPipeline::ProcessBatchPipe(verbose);
            return 0;
        }

        if (user.username.empty()) {
            std::cerr << "useradd: error: no username specified. Use --help for usage.\n";
            return 1;
        }

        // Handle reading password from standard input
        if (stdinPassword) {
            std::string pass;
            if (std::getline(std::cin, pass)) {
                if (!pass.empty() && pass.back() == '\r') pass.pop_back();
                user.password = pass;
            }
        }

        // Set default home directory if enabled and unspecified
        if (user.createHomeDir && user.homeDir.empty()) {
            user.homeDir = "C:\\Users\\" + user.username;
        }

        if (verbose) {
            std::cout << "Creating user: " << user.username << "\n";
            std::cout << "Home directory: " << (user.homeDir.empty() ? "(none)" : user.homeDir) << "\n";
            std::cout << "Primary group: " << user.primaryGroup << "\n";
        }

        std::string err;
        if (!WindowsUserManager::CreateAccount(user, err)) {
            std::cerr << "useradd: " << err << "\n";
            return 1;
        }

        if (verbose) {
            std::cout << "useradd: User '" << user.username << "' created successfully.\n";
        }

        return 0;
    }
