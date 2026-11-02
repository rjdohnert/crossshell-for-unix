// Application entry-point class for ipadm.
#include "ipadm_app.h"
#include "ipadm_help.h"
#include "ipadm_engine.h"
#include "ipadm_subcommands.h"

static void PrintVersion() {
    std::cout << "ipadm 1.1.0\n";
}

static int ipadm_main(int argc, char* argv[]) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    if (argc < 2) {
        PrintIpAdmHelp();
        return 0;
    }

    std::string subcommand = argv[1];
    if (subcommand == "help" || subcommand == "-h" || subcommand == "--help" || subcommand == "-?" || subcommand == "/?") {
        PrintIpAdmHelp();
        return 0;
    }
    if (subcommand == "--version" || subcommand == "-V") {
        PrintVersion();
        return 0;
    }

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    std::string targetArg = "";
    std::string typeArg = "static";
    std::string addrArg = "";
    std::string propArg = "";
    std::string outFormat = "table";
    std::string pipeCommand = "";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-T" && i + 1 < argc) {
            typeArg = argv[++i];
        } else if (arg == "-a" && i + 1 < argc) {
            addrArg = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            propArg = argv[++i];
        } else if (arg == "--json") {
            outFormat = "json";
        } else if (arg == "--csv") {
            outFormat = "csv";
        } else if (arg == "--table") {
            outFormat = "table";
        } else if (arg == "--pipe" && i + 1 < argc) {
            pipeCommand = argv[++i];
        } else if (arg[0] != '-') {
            targetArg = arg;
        }
    }

    auto interfaces = IpAdminEngine::DiscoverInterfaces();
    auto addrs = IpAdminEngine::DiscoverAddressObjects(interfaces);

    if (subcommand == "show-if") {
        ShowIf(interfaces, targetArg, outFormat);
    } else if (subcommand == "show-addr") {
        ShowAddr(addrs, targetArg, outFormat);
    } else if (subcommand == "show-prop") {
        ShowProp(propArg, targetArg);
    } else if (subcommand == "create-addr") {
        if (addrArg.empty() || targetArg.empty()) {
            std::cerr << "[-] Usage: ipadm create-addr -T <static|dhcp> -a <addr/prefix> <addrobj>\n";
            WSACleanup();
            return 1;
        }
        CreateAddress(typeArg, addrArg, targetArg, interfaces);
    } else if (subcommand == "delete-addr") {
        if (targetArg.empty()) {
            std::cerr << "[-] Usage: ipadm delete-addr <addrobj>\n";
            WSACleanup();
            return 1;
        }
        DeleteAddress(targetArg, addrs);
    } else if (subcommand == "create-if" || subcommand == "delete-if") {
        // Enabling/disabling the IP stack on an interface requires driver-level
        // changes not exposed by the IP Helper API. Report honestly.
        std::cerr << "ipadm: '" << subcommand << "' is not supported on Windows "
                  << "(interface enable/disable requires driver control). "
                  << "Use 'netsh interface set interface' instead.\n";
        WSACleanup();
        return 1;
    } else if (subcommand == "set-prop") {
        // Protocol property writes (forwarding, TTL) require netsh or registry
        // changes. Report honestly rather than claiming success.
        std::cerr << "ipadm: 'set-prop' is not supported on Windows via the IP Helper API. "
                  << "Use 'netsh interface ip set ...' or 'netsh int tcp set ...' instead.\n";
        WSACleanup();
        return 1;
    } else {
        std::cerr << "ipadm: invalid subcommand '" << subcommand << "'\n";
        std::cerr << "Try 'ipadm help' for more information.\n";
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}

int IpadmApplication::run(int argc, char* argv[]) const {
    return ipadm_main(argc, argv);
}
