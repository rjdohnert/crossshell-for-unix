#include "sudo_help.hpp"

void PrintUsage(const wchar_t* prog_name) {
    std::wcout << L"Usage: " << prog_name << L" [OPTIONS] <command> [args...]\n"
               << L"Execute a command with elevated privileges.\n\n"
               << L"Options:\n"
               << L"  -h, --help           Show this help text\n"
               << L"  -v, --version        Show version information\n"
               << L"      --validate       Validate the current environment and exit\n"
               << L"      --sudo-worker    Internal worker mode (used by the elevation flow)\n";
}

void PrintVersion() {
    std::wcout << L"sudo v1.0.0\n";
}
