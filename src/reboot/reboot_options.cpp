#include "reboot_options.hpp"

int RebootOptions::Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                showHelp = true;
                return 0;
            }
            if (arg == "--version") {
                showVersion = true;
                return 0;
            }
            if (arg == "--elevated") {
                elevatedRun = true;
                continue;
            }
            if (arg == "-n" || arg == "--no-wall") {
                suppressWall = true;
                continue;
            }
            if (arg == "-f" || arg == "--force") {
                force = true;
                continue;
            }
            if (arg == "-w" || arg == "--wtmp") {
                wtmpOnly = true;
                continue;
            }

            std::cerr << "reboot: unrecognized option '" << arg << "'\n";
            PrintUsage();
            return 2;
        }
        return -1;
    }

void RebootOptions::PrintUsage() const {
        std::cout << R"(reboot(1)               CrossShell for UNIX Reference Manual                reboot(1)

    NAME
        reboot - reboot, halt, or power off the Windows system

    SYNOPSIS
        reboot [OPTIONS]

    DESCRIPTION
        reboot initiates a system reboot using Windows shutdown privileges
        (SeShutdownPrivilege) and logging reasons.

    OPTIONS
        -f, --force
            Force immediate reboot without prompting or notifying users.

        -p, --poweroff
            Power down the machine instead of rebooting.

        -w, --wtmp-only
            Write reboot record to system logs without actually rebooting.

        -n, --no-wall
            Do not broadcast wall message to connected users.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        reboot
            Reboot local machine gracefully.

        reboot -f
            Force immediate system reboot.

    CrossShell for UNIX                                                 reboot(1)
)";
    }

void RebootOptions::PrintVersion() const {
        std::cout << "reboot v1.0.0\n";
    }
