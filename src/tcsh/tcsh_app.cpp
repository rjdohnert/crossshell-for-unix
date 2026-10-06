#include "tcsh_app.hpp"
#include "selftest.hpp"

int TcshApplication::Run(int argc, char* argv[]) {
    enable_ansi_support();

    TcshOptions options;
    OptionParser parser;
    if (!parser.Parse(argc, argv, options)) {
        return 1;
    }

    if (options.helpRequested) {
        TcshEngine shell(false);
        shell.displayHelp();
        return 0;
    }

    if (options.versionRequested) {
        TcshEngine shell(false);
        shell.displayVersion();
        return 0;
    }

    if (options.runSelfTests) {
        return runInternalSelfTests();
    }

    if (!options.commandString.empty()) {
        TcshEngine shell(options.loadRc);
        return shell.executeCommandString(options.commandString, options.scriptName, options.scriptArgs);
    }

    TcshEngine shell(options.loadRc);
    if (!options.scriptFile.empty()) {
        bool ok = shell.runScript(options.scriptFile, options.scriptArgs);
        return ok ? shell.getStatus() : 1;
    }

    shell.run();
    return 0;
}
