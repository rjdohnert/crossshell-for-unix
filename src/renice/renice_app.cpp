#include "name_filter.hpp"
#include "pid_filter.hpp"
#include "priority_mapper.hpp"
#include "privilege_manager.hpp"
#include "process_controller.hpp"
#include "process_filter.hpp"
#include "renice_app.hpp"
#include "user_filter.hpp"

ReniceApplication::ReniceApplication() {
        PrivilegeManager::enableDebugPrivilege();
    }

void ReniceApplication::printHelp(std::string_view /*execName*/ ) const {
        std::cout << R"(renice(1)                  CrossShell for UNIX Reference Manual                 renice(1)

    NAME
        renice - alter priority of running processes

    SYNOPSIS
        renice <priority> [OPTIONS] <target...>

    DESCRIPTION
        renice alters the scheduling priority class of one or more running
        processes on Windows. <priority> can be provided as an integer nice
        value (-20 to 20) or as a Windows priority class name (realtime, high,
        abovenormal, normal, belownormal, idle).

    OPTIONS
        -p, --pid <pid...>
            Interpret following arguments as Process IDs (default mode).

        -u, --user <user...>
            Alter all processes owned by the given user account name.

        -n, --name <name...>
            Alter all processes matching the executable image name (e.g., notepad.exe).

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        renice -5 4120 8912
            Set processes with PID 4120 and 8912 to AboveNormal priority (-5).

        renice 10 -n chrome.exe
            Lower all running instances of chrome.exe to Idle/Low priority.

        renice high -u Alice
            Set all processes owned by user Alice to High priority.

        renice normal -p 1044 -n svchost.exe
            Set PID 1044 and all svchost.exe instances back to Normal priority.

    CrossShell for UNIX                                                          renice(1)
)";
    }

int ReniceApplication::run(int argc, char* argv[]) {
        if (argc < 2) {
            printHelp(argv[0]);
            return 1;
        }

        std::string firstArg = argv[1];
        if (firstArg == "-h" || firstArg == "--help" || firstArg == "/?") {
            printHelp(argv[0]);
            return 0;
        }

        if (argc < 3) {
            std::cerr << "renice: error: missing target processes or priority.\n"
                      << "Try '" << argv[0] << " --help' for more information.\n";
            return 1;
        }

        // Parse Target Priority
        DWORD targetPriority = 0;
        if (auto p = PriorityMapper::fromString(firstArg)) {
            targetPriority = *p;
        } else {
            try {
                int niceVal = std::stoi(firstArg);
                if (auto pNice = PriorityMapper::fromNiceValue(niceVal)) {
                    targetPriority = *pNice;
                } else {
                    std::cerr << "renice: error: nice value " << niceVal << " is out of range [-20, 20].\n";
                    return 1;
                }
            } catch (...) {
                std::cerr << "renice: error: invalid priority specification: '" << firstArg << "'.\n";
                return 1;
            }
        }

        // Build target filters based on CLI flags
        enum class ParseMode { PID, USER, NAME };
        ParseMode mode = ParseMode::PID;

        std::vector<std::unique_ptr<IProcessFilter>> filters;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-p" || arg == "--pid") {
                mode = ParseMode::PID;
            } else if (arg == "-u" || arg == "--user") {
                mode = ParseMode::USER;
            } else if (arg == "-n" || arg == "--name") {
                mode = ParseMode::NAME;
            } else {
                if (mode == ParseMode::PID) {
                    try {
                        DWORD pid = static_cast<DWORD>(std::stoul(arg));
                        filters.push_back(std::make_unique<PidFilter>(pid));
                    } catch (...) {
                        std::cerr << "renice: error: invalid pid '" << arg << "'.\n";
                    }
                } else if (mode == ParseMode::USER) {
                    std::wstring wuser(arg.begin(), arg.end());
                    filters.push_back(std::make_unique<UserFilter>(wuser));
                } else if (mode == ParseMode::NAME) {
                    std::wstring wname(arg.begin(), arg.end());
                    filters.push_back(std::make_unique<NameFilter>(wname));
                }
            }
        }

        if (filters.empty()) {
            std::cerr << "renice: error: no valid target filters provided.\n";
            return 1;
        }

        auto processList = ProcessController::snapshotProcesses();
        int matchedCount = 0;
        int successCount = 0;

        for (const auto& proc : processList) {
            bool matched = false;
            for (const auto& filter : filters) {
                if (filter->matches(proc)) {
                    matched = true;
                    break;
                }
            }

            if (matched) {
                ++matchedCount;
                std::string err;
                std::string oldPrio = PriorityMapper::toString(proc.currentPriorityClass);
                std::string newPrio = PriorityMapper::toString(targetPriority);

                auto to_narrow = [](const std::wstring& wstr) -> std::string {
                    if (wstr.empty()) return {};
                    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
                    std::string result(size, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
                    return result;
                };

                if (ProcessController::applyPriority(proc.pid, targetPriority, err)) {
                    ++successCount;
                    std::cout << "[SUCCESS] PID " << std::setw(6) << proc.pid 
                              << " (" << to_narrow(proc.name) << "): "
                              << oldPrio << " -> " << newPrio << "\n";
                } else {
                    std::cerr << "[FAILED]  PID " << std::setw(6) << proc.pid 
                              << " (" << to_narrow(proc.name) << "): "
                              << err << "\n";
                }
            }
        }

        if (matchedCount == 0) {
            std::cerr << "renice: no matching processes found.\n";
            return 1;
        }

        return (matchedCount == successCount) ? 0 : 1;
    }
