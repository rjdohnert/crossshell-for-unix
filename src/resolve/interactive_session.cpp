#include "interactive_session.hpp"
#include "resolve_engine.hpp"

void InteractiveSession::Run(ResolveEngine& engine) {
        std::cout << "Default Server:  " << engine.GetServer() << "\n"
                  << "Address:  " << engine.GetServer() << "#53\n\n";

        std::string line;
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, line)) break;

            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty()) continue;

            if (line == "exit" || line == "quit") {
                break;
            } else if (line == "help" || line == "?") {
                std::cout << "Interactive Commands:\n"
                          << "  <host|ip>        Perform DNS query.\n"
                          << "  server <ip|name> Change DNS server.\n"
                          << "  set type=TYPE    Set query type (A, AAAA, MX, NS, PTR, SOA, TXT, ANY).\n"
                          << "  set debug        Enable debug logging.\n"
                          << "  set nodebug      Disable debug logging.\n"
                          << "  exit             Exit shell.\n\n";
            } else if (line.rfind("server ", 0) == 0) {
                std::string serverInput = line.substr(7);
                engine.SetServer(serverInput);
                std::cout << "Default Server:  " << engine.GetServer() << "\n"
                          << "Address:  " << engine.GetServer() << "#53\n\n";
            } else if (line.rfind("set type=", 0) == 0 || line.rfind("set q=", 0) == 0) {
                size_t eqPos = line.find('=');
                std::string qType = line.substr(eqPos + 1);
                if (!engine.SetQueryType(qType)) {
                    std::cout << "*** Unknown query type: " << qType << "\n";
                }
            } else if (line == "set debug") {
                engine.SetDebug(true);
            } else if (line == "set nodebug") {
                engine.SetDebug(false);
            } else {
                engine.ExecuteQuery(line);
                std::cout << "\n";
            }
        }
    }
