#include "netmap_app.hpp"

int NetmapApplication::Run(int argc, char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[-] Failed to initialize Windows Sockets layer.\n";
        return 1;
    }

    Netmap::NetmapOptions opts = Netmap::NetmapOptionsParser::Parse(argc, argv);
    if (!opts.valid) {
        std::cerr << "[-] Error: " << opts.errorMessage << "\n";
        WSACleanup();
        return 1;
    }

    bool success = true;
    switch (opts.mode) {
        case Netmap::Mode::Display:
            success = Netmap::ArpEngine::DisplayTable(opts.targetIp, opts.ifFilter, opts.verbose);
            break;
        case Netmap::Mode::Add:
            if (opts.targetIp && opts.targetMac) {
                success = Netmap::ArpEngine::AddEntry(*opts.targetIp, *opts.targetMac, opts.ifFilter);
            }
            break;
        case Netmap::Mode::Delete:
            if (opts.targetIp) {
                success = Netmap::ArpEngine::DeleteEntry(*opts.targetIp, opts.ifFilter);
            }
            break;
        case Netmap::Mode::Flush:
            success = Netmap::ArpEngine::FlushEntries(opts.ifFilter);
            break;
        case Netmap::Mode::Scan:
            if (opts.scanTarget) {
                success = Netmap::ArpEngine::ScanSubnet(*opts.scanTarget, opts.timeoutMs, opts.threadCount);
            }
            break;
        case Netmap::Mode::ListInterfaces:
            Netmap::HelpSystem::ShowInterfaces();
            break;
        case Netmap::Mode::Help:
            Netmap::HelpSystem::ShowHelp(opts.helpTopic);
            break;
        case Netmap::Mode::Version:
            Netmap::HelpSystem::ShowVersion();
            break;
    }

    WSACleanup();
    return success ? 0 : 1;
}
