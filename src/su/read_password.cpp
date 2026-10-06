#include "read_password.hpp"

std::wstring ReadPassword(const std::wstring& prompt) {
    std::wcout << prompt;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdin, &mode);
    
    // Disable character echo
    SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT);

    std::wstring password;
    std::getline(std::wcin, password);

    // Restore console mode
    SetConsoleMode(hStdin, mode);
    std::wcout << L"\n";
    return password;
}
