#include "gzip_app.hpp"

#include "gzip_cli.hpp"
#include "gzip_engine.hpp"

#include <iostream>
#include <string>

int GzipApp::run(int argc, wchar_t* argv[]) {
    GzipOptions options;
    std::wstring error;
    if (!GzipCli::parse(argc, argv, options, error)) {
        std::wcerr << L"gzip: " << error << L'\n'
                   << L"Use --help for usage information.\n";
        return 1;
    }
    if (options.showHelp) {
        GzipCli::printHelp();
        return 0;
    }
    if (options.showVersion) {
        GzipCli::printVersion();
        return 0;
    }
    return GzipEngine::execute(options);
}
