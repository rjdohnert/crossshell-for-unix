#include "handles_app.hpp"
#include "handles_cli.hpp"

#include <iostream>

int wmain(int argc, wchar_t* argv[]) {
    AppConfig config;
    std::wstring error;
    if (!HandlesCli::parse(argc, argv, config, error)) {
        std::wcerr << L"handles: " << error << L'\n';
        return 2;
    }

    if (config.showHelp) {
        HandlesCli::printHelp();
        return 0;
    }
    if (config.showVersion) {
        HandlesCli::printVersion();
        return 0;
    }
    return HandlesApp::run(config);
}
