#include "elevation_manager.hpp"
#include "privilege_manager.hpp"
#include "reboot_engine.hpp"
#include "reboot_options.hpp"

std::string RebootEngine::ToUpper(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return str;
    }

int RebootEngine::Execute(const RebootOptions& options) {
        SetConsoleTitleA("System Reboot");

        if (!options.suppressWall) {
            std::cout << "\n WARNING: You are about to reboot this System.\n\n";
        }

        std::cout << "Are you sure you want to proceed? (Y/N): ";
        std::string input;
        std::cin >> input;

        input = ToUpper(input);

        if (input == "Y" || input == "YES") {
            if (!PrivilegeManager::IsRunningAsAdmin() && !options.elevatedRun) {
                std::cout << "\nAdministrator rights are required to reboot this system." << std::endl;
                std::cout << "Showing UAC prompt for admin approval..." << std::endl;

                if (ElevationManager::RelaunchElevated()) {
                    std::cout << "Elevated instance launched." << std::endl;
                } else {
                    std::cerr << "Unable to request administrator privileges. Error code: " << GetLastError() << std::endl;
                }

                return 0;
            }

            if (options.wtmpOnly) {
                std::cout << "wtmp record update requested; no reboot performed." << std::endl;
                return 0;
            }

            std::cout << "\nAttempting to reboot the system..." << std::endl;

            if (PrivilegeManager::EnableShutdownPrivilege()) {
                UINT flags = EWX_REBOOT;
                if (options.force) flags |= EWX_FORCEIFHUNG;
                if (ExitWindowsEx(flags, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER)) {
                    return 0;
                } else {
                    std::cerr << "ExitWindowsEx failed. Error code: " << GetLastError() << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Failed to acquire necessary reboot privileges." << std::endl;
                return 1;
            }
        } else {
            std::cout << "\nReboot canceled." << std::endl;
            std::cout << "Press Enter to exit...";
            std::cin.ignore(10000, '\n');
            std::cin.get();
        }

        return 0;
    }
