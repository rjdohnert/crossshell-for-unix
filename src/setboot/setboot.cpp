/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "setboot.hpp"
#include "setboot_options.hpp"
#include "nvram_controller.hpp"
#include "boot_reporter.hpp"

class SetbootApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);

        bool hasEnvPrivilege = TokenPrivilegeGuard::enablePrivilege(L"SeSystemEnvironmentPrivilege");

        SetbootOptions opts;
        if (!SetbootOptions::parse(argc, argv, opts)) {
            return 1;
        }

        BootEnvironment env = UefiNvramController::queryEnvironment();

        if (!env.isUefi) {
            std::wcerr << L"Error: setboot requires a UEFI system. Current system mode: " << env.firmwareType << L"\n";
            return 1;
        }

        if (opts.isModify) {
            if (!hasEnvPrivilege) {
                std::wcerr << L"Error: Administrator privileges required to modify UEFI NVRAM parameters.\n";
                return 1;
            }

            if (!opts.setPrimary.empty()) {
                WORD targetId = 0;
                if (UefiNvramController::parseBootId(opts.setPrimary, targetId)) {
                    std::vector<WORD> order = env.bootOrder;
                    order.erase(std::remove(order.begin(), order.end(), targetId), order.end());
                    order.insert(order.begin(), targetId);

                    if (UefiNvramController::writeBootOrder(order)) {
                        std::wcout << L"Successfully set Primary Boot Path to Boot"
                                  << std::setfill(L'0') << std::setw(4) << std::hex << targetId << std::dec << L"\n";
                    } else {
                        std::wcerr << L"Failed to update BootOrder in NVRAM. Error: " << GetLastError() << L"\n";
                    }
                }
            }

            if (!opts.setAlternate.empty()) {
                WORD targetId = 0;
                if (UefiNvramController::parseBootId(opts.setAlternate, targetId)) {
                    if (UefiNvramController::writeBootNext(targetId)) {
                        std::wcout << L"Successfully set Boot Next Target to Boot"
                                  << std::setfill(L'0') << std::setw(4) << std::hex << targetId << std::dec << L"\n";
                    } else {
                        std::wcerr << L"Failed to set BootNext in NVRAM. Error: " << GetLastError() << L"\n";
                    }
                }
            }

            if (opts.setTimeout >= 0) {
                if (UefiNvramController::writeTimeout(static_cast<WORD>(opts.setTimeout))) {
                    std::wcout << L"Successfully set Autoboot Timeout to " << opts.setTimeout << L" seconds.\n";
                } else {
                    std::wcerr << L"Failed to set Timeout in NVRAM. Error: " << GetLastError() << L"\n";
                }
            }

            if (!opts.setAutoboot.empty()) {
                WORD timeoutVal = (opts.setAutoboot == L"ON" || opts.setAutoboot == L"ENABLE") ? 5 : 0;
                if (UefiNvramController::writeTimeout(timeoutVal)) {
                    std::wcout << L"Successfully set Autoboot to " << opts.setAutoboot << L"\n";
                } else {
                    std::wcerr << L"Failed to update Autoboot status in NVRAM. Error: " << GetLastError() << L"\n";
                }
            }

            env = UefiNvramController::queryEnvironment();
            std::wcout << L"\n";
        }

        SetbootReporter::display(env, opts.verbose);
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return SetbootApp::run(argc, argv);
}