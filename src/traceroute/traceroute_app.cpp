#include "network_utils.hpp"
#include "traceroute_app.hpp"
#include "traceroute_engine.hpp"
#include "traceroute_options.hpp"
#include "winsock_scope.hpp"

int TracerouteApplication::Run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);
        setlocale(LC_ALL, ".UTF-8");

        if (argc < 2) {
            TracerouteOptions opts;
            opts.PrintHelp(argv[0]);
            return 1;
        }

        TracerouteOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintHelp(argv[0]);
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintHelp(argv[0]);
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.targetHostStr.empty()) {
            std::cerr << "Error: Target host specification missing.\n";
            return 1;
        }

        if (!NetworkUtils::IsUserAdmin()) {
            std::cerr << "[!] WARNING: Administrator privileges required to capture raw ICMP responses.\n"
                      << "    Please run terminal session as Administrator.\n\n";
        }

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "Error: WSAStartup failed.\n";
            return 1;
        }

        TracerouteEngine engine(opts);
        return engine.Execute() ? 0 : 1;
    }
