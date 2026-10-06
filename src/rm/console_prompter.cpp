#include "console_prompter.hpp"

bool ConsolePrompter::Prompt(const std::string& message) {
        std::cerr << message << " (y/n)? ";
        std::cerr.flush();

        // If STDIN is redirected, read prompt response from physical console
        if (!_isatty(_fileno(stdin))) {
            HANDLE hConIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (hConIn != INVALID_HANDLE_VALUE) {
                char ch = 0;
                DWORD bytesRead = 0;
                ReadFile(hConIn, &ch, 1, &bytesRead, nullptr);
                CloseHandle(hConIn);
                std::cerr << "\n";
                return (ch == 'y' || ch == 'Y');
            }
        }

        std::string response;
        if (std::cin >> response) {
            return (!response.empty() && (response[0] == 'y' || response[0] == 'Y'));
        }
        return false;
    }
