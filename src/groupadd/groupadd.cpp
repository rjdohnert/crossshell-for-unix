#include "groupadd_app.hpp"
#include "groupadd_cli.hpp"

#include <iostream>

int wmain(int argc, wchar_t* argv[]) {
    CommandOptions options;
    std::wstring error;
    if (!CommandLineParser::Parse(argc, argv, options, error)) {
        std::wcerr << L"groupadd: " << error << L'\n'
                   << L"Try 'groupadd --help' for more information.\n";
        return 2;
    }

    if (options.showHelp) {
        CommandLineParser::PrintHelp();
        return 0;
    }
    if (options.showVersion) {
        CommandLineParser::PrintVersion();
        return 0;
    }

    return GroupAddApplication(std::move(options)).Run();
}
