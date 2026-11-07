#include "process_monitor.hpp"
#include "process_record.hpp"
#include "unique_handle.hpp"

#include <charconv>

namespace {
bool ParsePositiveInteger(const std::string& text, int& value) {
    if (text.empty()) return false;
    const char* begin = text.data();
    const char* end = begin + text.size();
    auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end && value > 0;
}
}

void HpUxTopEngine::HandleInput() {
    if (!_kbhit()) return;
    int ch = _getch();

    switch (ch) {
        case 0:
        case 224:
            _getch();
            break;
        case 'q':
        case 'Q':
        case 27:
        case 24:
            running = false;
            break;
        case 'h':
        case '?':
            RenderHelp();
            break;
        case 'c': currentSort = SortMode::CPU; break;
        case 'm': currentSort = SortMode::MEMORY; break;
        case 'p': currentSort = SortMode::PID; break;
        case 't': currentSort = SortMode::TIME; break;
        case 'a': currentSort = SortMode::NAME; break;
        case 'd': {
            std::string input = ReadPromptLine("Change delay interval (positive seconds): ");
            int delay = 0;
            if (ParsePositiveInteger(input, delay)) {
                refreshDelaySec = delay;
            } else {
                PromptLine("top: delay must be a positive whole number of seconds.");
                Sleep(1000);
            }
            break;
        }
        case 'n': {
            std::string input = ReadPromptLine("Number of processes to display: ");
            int count = 0;
            if (ParsePositiveInteger(input, count)) {
                maxDisplayCount = count;
            } else {
                PromptLine("top: row count must be a positive integer.");
                Sleep(1000);
            }
            break;
        }
        case 'u': {
            userFilter = ReadPromptLine("Filter by username (blank for all): ");
            if (userFilter == "\"\"" || userFilter == "none") {
                userFilter.clear();
            }
            break;
        }
        case 'k': {
            std::string input = ReadPromptLine("PID to terminate: ");
            int parsedPid = 0;
            if (!ParsePositiveInteger(input, parsedPid)) {
                PromptLine("top: PID must be a positive integer.");
                Sleep(1000);
                break;
            }

            DWORD targetPid = static_cast<DWORD>(parsedPid);
            UniqueHandle hKill = OpenProcess(PROCESS_TERMINATE, FALSE, targetPid);
            if (hKill.isValid() && TerminateProcess(hKill, 1)) {
                PromptLine("Successfully terminated PID " + std::to_string(targetPid) + ".");
            } else {
                PromptLine("Failed to terminate PID " + std::to_string(targetPid) +
                           ". Error: " + std::to_string(GetLastError()));
            }
            Sleep(1000);
            break;
        }
        default: break;
    }
}

void HpUxTopEngine::PromptLine(const std::string& msg) {
    std::cout << "\033[?25h\r\n\033[2K" << msg;
    std::cout.flush();
}

std::string HpUxTopEngine::ReadPromptLine(const std::string& msg) {
    PromptLine(msg);
    std::string input;
    if (!std::getline(std::cin, input)) {
        std::cin.clear();
    }
    std::cout << "\033[?25l";
    return input;
}
