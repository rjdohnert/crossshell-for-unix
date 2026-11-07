#include "groupmod_app.hpp"
#include "groupmod_cli.hpp"

#include <iostream>
#include <string>
#include <utility>

int wmain(int argc, wchar_t* argv[]) {
    CommandOptions options;
    std::wstring error;
    if (!CommandLineParser::Parse(argc, argv, options, error)) {
        std::wcerr << L"groupmod: " << error << L'\n'
                   << L"Try 'groupmod --help' for more information.\n";
        return 2;
    }

    GroupModApplication application(std::move(options));
    return application.Run();
}
