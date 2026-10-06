#include "nohup_app.hpp"

int NohupApplication::Run(int argc, wchar_t* argv[]) const {
    NohupOptions options;
    if (!options.Parse(argc, argv)) {
        std::wcerr << L"nohup: missing operand\n"
                  << L"Try 'nohup --help' for more information.\n";
        return 127;
    }

    if (options.showHelp) {
        options.PrintHelp();
        return 0;
    }

    if (options.showVersion) {
        options.PrintVersion();
        return 0;
    }

    return DetachedProcessLauncher::Launch(argc, argv, options);
}
