#include "nodename_app.hpp"

int NodenameApplication::Run(int argc, wchar_t* argv[]) {
    NodenameOptions opts;
    if (!opts.Parse(argc, argv)) {
        opts.PrintUsage(argc > 0 ? argv[0] : L"nodename");
        return 1;
    }

    if (opts.showHelp) {
        opts.PrintUsage(argc > 0 ? argv[0] : L"nodename");
        return 0;
    }

    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    if (opts.hasNewHostname) {
        DWORD err = 0;
        if (SystemNodeManager::SetNodeName(opts.newHostname, err)) {
            std::wcout << L"Hostname successfully changed to '" << opts.newHostname << L"'.\n";
            std::wcout << L"You must restart the computer for the changes to take effect.\n";
            return 0;
        } else {
            std::wcerr << L"nodename: failed to set hostname. Error code: " << err << L"\n";
            if (err == ERROR_ACCESS_DENIED) {
                std::wcerr << L"Error: Access Denied. Please run this command as Administrator.\n";
            }
            return 1;
        }
    }

    std::wstring name = SystemNodeManager::QueryName(opts.mode);
    if (name.empty() && opts.mode != NameQueryMode::DomainOnly) {
        std::wcerr << L"nodename: failed to retrieve hostname.\n";
        return 1;
    }

    std::wcout << name << std::endl;
    return 0;
}
