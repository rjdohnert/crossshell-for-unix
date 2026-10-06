#include "boot_reporter.hpp"

void SetbootReporter::display(const BootEnvironment& env, bool verbose) {
    std::wcout << L"\nSystem Boot Parameters\n\n";
    std::wcout << L"Firmware Mode:          " << env.firmwareType << L"\n";

    if (!env.bootOrderReadOk) {
        std::wcerr << L"Warning: Unable to read UEFI BootOrder variable (Error: " << env.bootOrderReadError << L").\n";
        if (env.bootOrderReadError == ERROR_PRIVILEGE_NOT_HELD || env.bootOrderReadError == ERROR_ACCESS_DENIED) {
            std::wcerr << L"         Run an elevated Administrator terminal to query firmware NVRAM entries.\n";
        }
    }

    // Primary Boot Path
    if (!env.bootEntries.empty()) {
        const auto& primary = env.bootEntries[0];
        std::wcout << L"Primary Boot Path:      " << primary.name << L" [" << primary.description << L"]\n";
        if (verbose) {
            std::wcout << L"                        " << primary.devicePath << L"\n";
        }
    } else {
        std::wcout << L"Primary Boot Path:      None\n";
    }

    // Alternate Boot Path
    if (env.bootEntries.size() > 1) {
        const auto& alt = env.bootEntries[1];
        std::wcout << L"Alternate Boot Path:    " << alt.name << L" [" << alt.description << L"]\n";
        if (verbose) {
            std::wcout << L"                        " << alt.devicePath << L"\n";
        }
    } else {
        std::wcout << L"Alternate Boot Path:    None\n";
    }

    // Boot Next Target
    if (env.hasNextBoot) {
        std::wcout << L"Boot Next Target:       Boot" << std::setfill(L'0') << std::setw(4) << std::hex << env.nextBootId << std::dec << L"\n";
    } else {
        std::wcout << L"Boot Next Target:       None (Default Sequence)\n";
    }

    // Autoboot / Timeout Status
    std::wcout << L"Autoboot Status:        " << ((env.timeoutSeconds > 0) ? L"ENABLED" : L"DISABLED") << L"\n";
    std::wcout << L"Autoboot Timeout:       " << env.timeoutSeconds << L" seconds\n\n";

    // Boot Order Table
    std::wcout << L"Boot Order Sequence:\n";
    std::wcout << L"--------------------------------------------------------------------------------\n";

    for (size_t i = 0; i < env.bootEntries.size(); ++i) {
        const auto& entry = env.bootEntries[i];
        bool isCurrent = (env.hasCurrentBoot && entry.id == env.currentBootId);

        std::wcout << L"  " << std::setw(2) << (i + 1) << L". "
                  << entry.name << L" : "
                  << entry.description
                  << (isCurrent ? L" [Active Boot]" : L"") << L"\n";

        if (verbose) {
            std::wcout << L"      Path: " << entry.devicePath << L"\n"
                      << L"      Attr: 0x" << std::hex << entry.attributes << std::dec << L"\n";
        }
    }
}
