#include "netctl_app.hpp"

int NetctlApplication::Run(int argc, char* argv[]) {
    EnableVT100Colors();
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[!] WSAStartup failed." << std::endl;
        return 1;
    }

    NetctlOptions opts = NetctlOptionsParser::Parse(argc, argv);

    if (opts.command == NetctlCommand::Help) {
        NetctlOptionsParser::DisplayHelp();
        WSACleanup();
        return 0;
    }

    if (opts.command == NetctlCommand::Version) {
        NetctlOptionsParser::DisplayVersion();
        WSACleanup();
        return 0;
    }

    if (opts.command == NetctlCommand::List) {
        auto interfaces = ListNetworkInterfaces();
        std::cout << Color::BOLD << Color::CYAN << "\n[+] Available Active Network Interfaces:\n" << Color::RESET;
        std::cout << "------------------------------------------------------------------------------\n";
        int idx = 1;
        for (const auto& iface : interfaces) {
            std::cout << "  [" << idx++ << "] Host: " << std::left << std::setw(28) << iface.first 
                      << " Address: " << Color::GREEN << iface.second << Color::RESET << "\n";
        }
        std::cout << "------------------------------------------------------------------------------\n";
        std::cout << "Use IP with: netctl capture -i <IP>\n\n";
        WSACleanup();
        return 0;
    }

    if (opts.command == NetctlCommand::Capture) {
        if (!opts.valid) {
            std::cerr << Color::RED << "[!] Invalid argument: " << opts.error_message << Color::RESET << std::endl;
            WSACleanup();
            return 1;
        }

        if (!IsUserAdmin()) {
            std::cerr << Color::RED << "\n[!] ERROR: Administrator Privileges Required!" << Color::RESET << "\n"
                      << "    Raw Socket capture (SIO_RCVALL) requires elevated rights.\n"
                      << "    Please re-run Command Prompt or PowerShell as Administrator.\n\n";
            WSACleanup();
            return 1;
        }

        Logger logger(opts.config.output_file, opts.config.no_color);
        if (opts.config.use_npcap) {
            StartNpcapCapture(opts.config, logger);
        } else {
            StartCaptureEngine(opts.config, logger);
        }
        WSACleanup();
        return 0;
    }

    if (!opts.valid) {
        std::cerr << Color::RED << "[!] Error: " << opts.error_message << Color::RESET << std::endl;
        WSACleanup();
        return 1;
    }

    std::cerr << Color::RED << "[!] Unknown command: " << opts.unknown_command << Color::RESET << "\nRun 'netctl help' for usage.\n";
    WSACleanup();
    return 1;
}
