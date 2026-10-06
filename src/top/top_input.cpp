#include "process_monitor.hpp"
#include "process_record.hpp"
#include "unique_handle.hpp"

void HpUxTopEngine::HandleInput() {
        if (!_kbhit()) return;
        int ch = _getch();

        switch (ch) {
            case 'q':
            case 'Q':
                std::cout << "\033[?25h\033[0m\nTerminated by user.\n";
                exit(0);
            case 'h':
            case '?':
                RenderHelp();
                break;
            case 'c': currentSort = SortMode::CPU; break;
            case 'm': currentSort = SortMode::MEMORY; break;
            case 'p': currentSort = SortMode::PID; break;
            case 't': currentSort = SortMode::TIME; break;
            case 'd': {
                PromptLine("Change delay interval (seconds): ");
                int delay = 0;
                std::cin >> delay;
                if (delay > 0) refreshDelaySec = delay;
                break;
            }
            case 'n': {
                PromptLine("Number of processes to display: ");
                int n = 0;
                std::cin >> n;
                if (n > 0) maxDisplayCount = n;
                break;
            }
            case 'u': {
                PromptLine("Filter by username (blank for all): ");
                std::string u;
                std::getline(std::cin >> std::ws, u);
                userFilter = (u == "\"\"" || u == "none") ? "" : u;
                break;
            }
            case 'k': {
                PromptLine("PID to kill: ");
                DWORD targetPid = 0;
                std::cin >> targetPid;
                if (targetPid > 0) {
                    UniqueHandle hKill = OpenProcess(PROCESS_TERMINATE, FALSE, targetPid);
                    if (hKill.isValid() && TerminateProcess(hKill, 1)) {
                        PromptLine("Successfully terminated PID " + std::to_string(targetPid) + ".");
                    } else {
                        PromptLine("Failed to terminate PID " + std::to_string(targetPid) + ". Error: " + std::to_string(GetLastError()));
                    }
                    Sleep(1000);
                }
                break;
            }
            default: break;
        }
    }

void HpUxTopEngine::PromptLine(const std::string& msg) {
        std::cout << "\033[?25h"; // Show cursor
        std::cout << "\033[24;1H\033[2K" << msg;
        std::cout.flush();
    }
