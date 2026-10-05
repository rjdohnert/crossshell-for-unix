#include "lsuser_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <locale>

int LsuserApplication::Run(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    std::wcout.imbue(std::locale(""));

    LsuserOptions opts;
    if (!opts.Parse(argc, argv)) {
        opts.PrintUsage(argc > 0 ? argv[0] : "lsuser");
        return 1;
    }

    if (opts.showHelp) {
        opts.PrintUsage(argc > 0 ? argv[0] : "lsuser");
        return 0;
    }

    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    std::vector<AccountRecord> accounts = UserAccountEnumerator::EnumerateLocalAccounts();
    if (accounts.empty()) {
        std::wcerr << L"lsuser: unable to enumerate local accounts" << std::endl;
        return 1;
    }

    if (!opts.requestedUsers.empty()) {
        std::vector<AccountRecord> filtered;
        for (const auto& account : accounts) {
            std::wstring lowerName = UserAccountEnumerator::ToLower(account.name);
            for (const auto& wanted : opts.requestedUsers) {
                if (lowerName == UserAccountEnumerator::ToLower(wanted)) {
                    filtered.push_back(account);
                    break;
                }
            }
        }
        accounts.swap(filtered);
    }

    if (accounts.empty()) {
        return 0;
    }

    LsuserReporter::Emit(opts, accounts);
    return 0;
}
