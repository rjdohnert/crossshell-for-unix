#include "interactive_session.hpp"
#include "resolve_app.hpp"
#include "resolve_engine.hpp"
#include "string_encoding.hpp"
#include "winsock_scope.hpp"

void ResolveApplication::PrintHelp(const wchar_t* progName) const {
           std::wcout << LR"HELP(resolve(1)               CrossShell for UNIX Reference Manual                  resolve(1)

    NAME
        resolve - query DNS records and reverse DNS names

    SYNOPSIS
        resolve [OPTIONS] [HOST|IP] [DNS_SERVER]

    DESCRIPTION
        Resolves hostnames, IPv4 addresses, and IPv6 addresses using DNS. With no
        target or with '-' as the target, resolve starts an interactive session.

    OPTIONS
        -type=TYPE, -querytype=TYPE
            Select A, AAAA, MX, NS, PTR, SOA, TXT, or ANY records.
        -debug                 Enable verbose DNS header and query output.
        -nodebug               Disable debug output (default).
        -?, -h, --help         Display this comprehensive reference manual and exit.

    INTERACTIVE COMMANDS
        HOST|IP                Perform a query.
        server IP|NAME         Change the DNS server.
        set type=TYPE          Change the query type.
        set q=TYPE             Change the query type.
        set debug/nodebug      Toggle debug output.
        help, ?, exit, quit    Display help or leave the session.

    EXAMPLES
        resolve example.com
        resolve -type=MX example.com 8.8.8.8
        resolve 8.8.8.8
        resolve -

    EXIT STATUS
        0          Help, interactive termination, or completed query.
        1          Winsock initialization failure.

    CrossShell for UNIX                                                      resolve(1)
    )HELP";
           return;

        std::wcout << L"Usage: " << progName << L" [-option ...] [host-to-find | - [server]]\n\n"
                   << L"Queries DNS domain name servers interactively or in non-interactive mode.\n\n"
                   << L"Non-Interactive Flags:\n"
                   << L"  -type=TYPE      Set record query type (A, AAAA, MX, NS, PTR, SOA, TXT, ANY).\n"
                   << L"  -querytype=TYPE Synonym for -type.\n"
                   << L"  -debug          Enable verbose DNS debug header logging.\n"
                   << L"  -nodebug        Disable verbose debug output (default).\n"
                   << L"  -?, -h, --help  Display this comprehensive help menu.\n\n"
                   << L"Interactive Mode Commands (type 'resolve' with no args to launch):\n"
                   << L"  <host|ip>        Look up the specified hostname or IP address.\n"
                   << L"  server <ip|name> Change default DNS server to IP or hostname.\n"
                   << L"  set type=TYPE    Change record query type (A, MX, NS, PTR, etc.).\n"
                   << L"  set debug        Enable debug logging in interactive mode.\n"
                   << L"  set nodebug      Disable debug logging.\n"
                   << L"  exit / quit      Exit interactive mode.\n"
                   << L"  help / ?         Show interactive help.\n\n";
    }

int ResolveApplication::Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);
        setlocale(LC_ALL, ".UTF-8");

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "Error: WSAStartup failed.\n";
            return 1;
        }

        ResolveEngine engine;
        std::string targetHost = "";

        for (int i = 1; i < argc; ++i) {
            std::string arg = StringEncoding::WideToUtf8(argv[i]);
            if (arg == "-?" || arg == "-h" || arg == "--help") {
                PrintHelp(argv[0]);
                return 0;
            } else if (arg.rfind("-type=", 0) == 0) {
                engine.SetQueryType(arg.substr(6));
            } else if (arg.rfind("-querytype=", 0) == 0) {
                engine.SetQueryType(arg.substr(11));
            } else if (arg == "-debug") {
                engine.SetDebug(true);
            } else if (arg == "-nodebug") {
                engine.SetDebug(false);
            } else if (!arg.empty() && arg[0] != '-') {
                if (targetHost.empty()) {
                    targetHost = arg;
                } else {
                    engine.SetServer(arg);
                }
            }
        }

        if (targetHost.empty() || targetHost == "-") {
            InteractiveSession::Run(engine);
        } else {
            engine.ExecuteQuery(targetHost);
        }

        return 0;
    }
