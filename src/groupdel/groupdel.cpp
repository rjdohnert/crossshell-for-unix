#include "groupdel_app.hpp"
#include "groupdel_cli.hpp"

#include <iostream>
#include <string>
#include <utility>

int wmain(int argc, wchar_t* argv[]) {
    CommandOptions options;
    std::wstring error;
    if (!CommandLineParser::Parse(argc, argv, options, error)) {
        std::wcerr << L"groupdel: " << error << L'\n'
                   << L"Try 'groupdel --help' for more information.\n";
        return 2;
    }

    GroupDelApplication application(std::move(options));
    return application.Run();
}
