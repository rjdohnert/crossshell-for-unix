#include "options.hpp"

bool OptionParser::Parse(int argc, char* argv[], TcshOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "-help") {
            options.helpRequested = true;
            return true;
        }

        if (arg == "-v" || arg == "--version" || arg == "-version") {
            options.versionRequested = true;
            return true;
        }

        if (arg == "--self-test") {
            options.runSelfTests = true;
            return true;
        }

        if (arg == "-f" || arg == "--no-rcs" || arg == "-fast") {
            options.loadRc = false;
            continue;
        }

        if (arg == "-c") {
            if (i + 1 < argc) {
                options.commandString = argv[++i];
                if (i + 1 < argc) {
                    options.scriptName = argv[++i];
                }
                for (int j = i + 1; j < argc; ++j) {
                    options.scriptArgs.push_back(argv[j]);
                }
                return true;
            } else {
                print_error_message("tcsh: -c requires an argument\n");
                return false;
            }
        }

        if (arg == "--") {
            if (i + 1 < argc) {
                options.scriptFile = argv[++i];
                for (int j = i + 1; j < argc; ++j) {
                    options.scriptArgs.push_back(argv[j]);
                }
            }
            break;
        }

        // Script file passed as argument
        options.scriptFile = arg;
        for (int j = i + 1; j < argc; ++j) {
            options.scriptArgs.push_back(argv[j]);
        }
        break;
    }

    return true;
}
