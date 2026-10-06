#include "options.hpp"

void NcOptions::ParsePorts(const std::string& arg) {
    size_t dash = arg.find('-');
    if (dash != std::string::npos) {
        try {
            int start = std::stoi(arg.substr(0, dash));
            int end = std::stoi(arg.substr(dash + 1));
            for (int p = start; p <= end; ++p) {
                ports.push_back(p);
            }
        } catch (...) {}
    } else {
        try {
            ports.push_back(std::stoi(arg));
        } catch (...) {
            struct servent* se = getservbyname(arg.c_str(), NULL);
            if (se) ports.push_back(ntohs(se->s_port));
        }
    }
}

bool NcOptions::Parse(int argc, char* argv[]) {
    auto get_opt_value = [&](char flag, size_t& j, int& i) -> std::string {
        if (j + 1 < std::string(argv[i]).length()) {
            std::string val = std::string(argv[i]).substr(j + 1);
            j = std::string(argv[i]).length() - 1;
            return val;
        } else if (i + 1 < argc) {
            return std::string(argv[++i]);
        } else {
            std::cerr << "nc: option requires an argument -- " << flag << "\n";
            return "";
        }
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--help" || arg == "/?" || arg == "-h") {
            showHelp = true;
            return true;
        }
        if (arg == "--version" || arg == "-V") {
            showVersion = true;
            return true;
        }
        if (arg.length() > 1 && arg[0] == '-') {
            for (size_t j = 1; j < arg.length(); ++j) {
                char c = arg[j];
                if (c == '4') ipv4_only = true;
                else if (c == '6') ipv6_only = true;
                else if (c == 'd') detach_stdin = true;
                else if (c == 'h') { showHelp = true; return true; }
                else if (c == 'k') keep_open = true;
                else if (c == 'l') listen = true;
                else if (c == 'n') no_dns = true;
                else if (c == 'u') udp = true;
                else if (c == 'v') verbose++;
                else if (c == 'z') zero_io = true;
                else if (c == 'e') {
                    exec_cmd = get_opt_value(c, j, i);
                    if (exec_cmd.empty()) return false;
                    break;
                }
                else if (c == 'i') {
                    std::string v = get_opt_value(c, j, i);
                    if (v.empty()) return false;
                    interval = std::stoi(v);
                    break;
                }
                else if (c == 'p') {
                    std::string v = get_opt_value(c, j, i);
                    if (v.empty()) return false;
                    local_port = std::stoi(v);
                    break;
                }
                else if (c == 's') {
                    source_ip = get_opt_value(c, j, i);
                    if (source_ip.empty()) return false;
                    break;
                }
                else if (c == 'w') {
                    std::string v = get_opt_value(c, j, i);
                    if (v.empty()) return false;
                    timeout = std::stoi(v);
                    break;
                }
                else {
                    std::cerr << "nc: invalid option -- " << c << "\n";
                    return false;
                }
            }
        } else {
            if (destination.empty() && !listen) destination = arg;
            else ParsePorts(arg);
        }
    }

    if (listen && local_port == 0 && !destination.empty()) {
        try { local_port = std::stoi(destination); destination = ""; } catch (...) {}
    }

    return true;
}

void NcOptions::PrintHelp() const {
    std::cout << R"(nc(1)                   CrossShell for UNIX Reference Manual                    nc(1)

    NAME
        nc - arbitrary TCP and UDP connections and listens (netcat)

    SYNOPSIS
        nc [OPTIONS] HOST PORT...
        nc -l [OPTIONS] [-p PORT] [HOST] [PORT]

    DESCRIPTION
        The nc (or netcat) utility is used for just about anything under the sun
        involving TCP or UDP. It can open TCP connections, send UDP packets,
        listen on arbitrary TCP/UDP ports, do port scanning, and deal with both
        IPv4 and IPv6.

    OPTIONS
        -l, --listen
            Listen for an incoming connection rather than initiating one.

        -p PORT, --port PORT
            Specify local source port.

        -u, --udp
            Use UDP instead of default TCP.

        -v, --verbose
            Enable verbose progress diagnostics.

        -z, --zero-io
            Zero-I/O mode (used for port scanning).

        -w SECONDS, --timeout SECONDS
            Timeout for connects and final network reads.

        -k, --keep-open
            Force nc to stay listening for another connection after current connection is closed.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        nc -zv host.example.com 20-30
            Port-scan TCP ports 20 through 30 on host.example.com.

        nc -l -p 8080
            Listen on local port 8080 for incoming TCP connections.

    CrossShell for UNIX                                                     nc(1)
)";
}

void NcOptions::PrintVersion() const {
    std::cout << "nc 3.4.7\n";
}
