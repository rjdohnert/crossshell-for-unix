#include "whois_app.hpp"
#include "whois_client.hpp"
#include "whois_options.hpp"
#include "whois_response_parser.hpp"
#include "winsock_scope.hpp"

int WhoisApplication::Run(int argc, char* argv[]) {
        WhoisOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : "whois");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : "whois");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "whois: Winsock initialization failed\n";
            return 1;
        }

        std::string currentServer = opts.server;
        std::vector<std::string> visitedServers;
        int maxReferrals = 4;

        while (!currentServer.empty() && maxReferrals-- > 0) {
            if (std::find(visitedServers.begin(), visitedServers.end(), currentServer) != visitedServers.end()) {
                break;
            }
            visitedServers.push_back(currentServer);

            std::cout << "[Querying " << currentServer << ":" << opts.port << " for '" << opts.target << "']\n\n";

            std::string response = WhoisClient::QueryServer(currentServer, opts.port, opts.target);

            if (response.rfind("Error:", 0) == 0) {
                std::cerr << response << "\n";
                return 1;
            }

            std::cout << response << "\n";

            if (!opts.autoFollow) {
                break;
            }

            std::string nextServer = WhoisResponseParser::ExtractReferralServer(response);
            if (!nextServer.empty() && nextServer != currentServer) {
                std::cout << "\n--------------------------------------------------\n";
                std::cout << "Following referral to " << nextServer << "...\n";
                std::cout << "--------------------------------------------------\n\n";
                currentServer = nextServer;
            } else {
                break;
            }
        }

        return 0;
    }
