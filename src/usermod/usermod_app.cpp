#include "command_line_parser.hpp"
#include "input_pipeline.hpp"
#include "mod_options.hpp"
#include "string_utils.hpp"
#include "usermod_app.hpp"
#include "windows_account_modifier.hpp"

int Application::Run(int argc, char* argv[]) {
        if (argc < 2) {
            if (InputPipeline::IsPipeActive()) {
                ModOptions defaultOpt;
                return InputPipeline::ProcessBatchPipe(defaultOpt);
            }
            CommandLineParser::PrintHelp();
            return 2;
        }

        ModOptions opt;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                CommandLineParser::PrintHelp();
                return 0;
            } else if (arg == "--version") {
                CommandLineParser::PrintVersion();
                return 0;
            } else if (arg == "-v" || arg == "--verbose") {
                opt.verbose = true;
            } else if (arg == "--pipe") {
                opt.pipeMode = true;
            } else if (arg == "--stdin-password") {
                opt.stdinPassword = true;
            } else if (arg == "-a" || arg == "--append") {
                opt.appendGroups = true;
            } else if (arg == "-m" || arg == "--move-home") {
                opt.moveHome = true;
            } else if (arg == "-L" || arg == "--lock") {
                opt.lockAccount = true;
            } else if (arg == "-U" || arg == "--unlock") {
                opt.lockAccount = false;
            } else if (arg == "-f" || arg == "--force-password-change") {
                opt.mustChangePassword = true;
            } else if (arg == "--never-expires") {
                opt.passwordNeverExpires = true;
            } else if (arg == "--expires") {
                opt.passwordNeverExpires = false;
            } else if (arg == "-c" || arg == "--comment") {
                if (++i < argc) opt.comment = argv[i];
            } else if (arg == "--full-name") {
                if (++i < argc) opt.fullName = argv[i];
            } else if (arg == "-d" || arg == "--home-dir") {
                if (++i < argc) opt.homeDir = argv[i];
            } else if (arg == "-g" || arg == "--gid" || arg == "--group") {
                if (++i < argc) opt.primaryGroup = argv[i];
            } else if (arg == "-G" || arg == "--groups") {
                if (++i < argc) opt.supplementaryGroups = StringUtils::Split(argv[i], ',');
            } else if (arg == "-l" || arg == "--login") {
                if (++i < argc) opt.newLogin = argv[i];
            } else if (arg == "-p" || arg == "--password") {
                if (++i < argc) opt.password = argv[i];
            } else if (!arg.empty() && arg[0] != '-') {
                opt.username = arg;
            } else {
                std::cerr << "usermod: unrecognized option '" << arg << "'\n";
                std::cerr << "Try 'usermod --help' for more information.\n";
                return 2;
            }
        }

        if (opt.pipeMode) {
            return InputPipeline::ProcessBatchPipe(opt);
        }

        if (opt.username.empty()) {
            std::cerr << "usermod: error: no username specified. Use --help for usage.\n";
            return 2;
        }

        // Handle reading password from standard input pipe
        if (opt.stdinPassword) {
            std::string pass;
            if (std::getline(std::cin, pass)) {
                opt.password = StringUtils::Trim(pass);
            }
        }

        if (opt.verbose) {
            std::cout << "Target User: " << opt.username << "\n";
            if (opt.newLogin.has_value()) std::cout << "  New Login: " << opt.newLogin.value() << "\n";
            if (opt.homeDir.has_value()) std::cout << "  New Home Dir: " << opt.homeDir.value() << "\n";
            if (opt.lockAccount.has_value()) std::cout << "  Account Lock: " << (opt.lockAccount.value() ? "Enabled" : "Disabled") << "\n";
        }

        std::string err;
        if (!WindowsAccountModifier::ModifyAccount(opt, err)) {
            std::cerr << "usermod: " << err << "\n";
            return 1;
        }

        if (opt.verbose) {
            std::cout << "usermod: Account '" << opt.username << "' modified successfully.\n";
        }

        return 0;
    }
