#include "wall_config.hpp"

void WallConfig::printHelp(const char* exeName) {
        std::cout <<
R"(NAME
    wall - write a message to all logged-in users on Windows

SYNOPSIS
    )" << exeName << R"( [OPTIONS] [FILE]
    )" << exeName << R"( [OPTIONS] [MESSAGE...]
    echo "Message" | )" << exeName << R"( [OPTIONS]

DESCRIPTION
    wall displays a message, or the contents of a file, or lines taken from its
    standard input, on the terminals and active sessions of all currently logged-in
    users.

OPTIONS
    -n, --nobanner
        Suppress the standard header banner showing user, host, and time.
    -g, --gui
        Display the message as an interactive graphical dialog on user desktops.
    -c, --conhost
        Force delivery directly to active console hosts.
    --server SERVER
        Broadcast to a remote Terminal Server / Windows Host.
    --json, --csv, --table
        Format delivery confirmation records as JSON, CSV, or Table.
    --pipe COMMAND
        Forward delivery receipts through COMMAND.
    -h, --help
        Display this help manual.
    -V, --version
        Display version information.

EXAMPLES
    wall System going down for maintenance in 5 minutes!
    type notice.txt | wall -n
    wall -g --server RDS-HOST-01 Update scheduled at 22:00.
)";
    }

void WallConfig::printVersion() {
        std::cout << "wall version " << VERSION << "\n"
                  << "Copyright (c) 2026, Roberto J Dohnert.\n";
    }

bool WallConfig::parse(int argc, char* argv[], WallConfig& cfg) {
        std::vector<std::string> words;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-n" || arg == "--nobanner") {
                cfg.noBanner = true;
            } else if (arg == "-g" || arg == "--gui") {
                cfg.guiPopup = true;
            } else if (arg == "-c" || arg == "--conhost") {
                cfg.useConhost = true;
            } else if (arg == "--json") {
                cfg.outputFormat = 1;
            } else if (arg == "--csv") {
                cfg.outputFormat = 2;
            } else if (arg == "--table") {
                cfg.outputFormat = 3;
            } else if (arg == "--server" && i + 1 < argc) {
                cfg.serverName = argv[++i];
            } else if (arg == "--pipe" && i + 1 < argc) {
                cfg.pipeCommand = argv[++i];
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "wall: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                words.push_back(arg);
            }
        }

        if (!words.empty()) {
            std::ifstream testFile(words[0]);
            if (words.size() == 1 && testFile.good()) {
                cfg.filePath = words[0];
            } else {
                std::ostringstream oss;
                for (size_t k = 0; k < words.size(); ++k) {
                    if (k > 0) oss << " ";
                    oss << words[k];
                }
                cfg.inlineMessage = oss.str();
            }
        }

        return true;
    }
