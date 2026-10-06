#include "options.hpp"

bool NohupOptions::Parse(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        return false;
    }

    std::wstring first_arg = argv[1];
    if (first_arg == L"--help" || first_arg == L"-h" || first_arg == L"/?") {
        showHelp = true;
        return true;
    }
    if (first_arg == L"--version" || first_arg == L"-v") {
        showVersion = true;
        return true;
    }

    commandIndex = 1;
    return true;
}

void NohupOptions::PrintHelp() const {
    std::wcout << L"NAME\n"
              << L"    nohup - run a command immune to hangups, with output to a non-tty\n\n"
              << L"SYNOPSIS\n"
              << L"    nohup COMMAND [ARGUMENT...]\n"
              << L"    nohup OPTION\n\n"
              << L"DESCRIPTION\n"
              << L"    Run COMMAND, ignoring hangup signals (Console Close, Ctrl+C, Ctrl+Break,\n"
              << L"    Logoff, and Shutdown events).\n\n"
              << L"    If standard input is a terminal/console, it is redirected from NUL.\n"
              << L"    If standard output is a terminal/console, output is appended to 'nohup.out'.\n"
              << L"    If 'nohup.out' cannot be written in current dir, output is appended to\n"
              << L"    '%USERPROFILE%\\nohup.out' or '%TEMP%\\nohup.out'.\n"
              << L"    If standard error is a terminal/console, it is redirected to standard output.\n\n"
              << L"OPTIONS\n"
              << L"    --help, -h, /?    Display this comprehensive help message and exit.\n"
              << L"    --version, -v     Output version information and exit.\n\n"
              << L"EXIT STATUS\n"
              << L"    126               COMMAND was found but could not be invoked.\n"
              << L"    127               COMMAND could not be found or an internal error occurred.\n"
              << L"    Otherwise         The exit status of COMMAND.\n\n"
              << L"EXAMPLES\n"
              << L"    nohup my_script.bat\n"
              << L"    nohup python long_job.py \"arg with spaces\" 100\n"
              << L"    nohup ping 127.0.0.1 -t > custom_log.txt 2>&1\n";
}

void NohupOptions::PrintVersion() const {
    std::wcout << L"nohup 1.0.0\n";
}
