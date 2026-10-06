#include "options.hpp"

bool NetctlOptionsParser::ParsePortValue(const std::string& text, uint16_t& out_port) {
    unsigned long raw = std::stoul(text);
    if (raw < 1 || raw > 65535) {
        throw std::out_of_range("port out of range (1-65535)");
    }
    out_port = static_cast<uint16_t>(raw);
    return true;
}

bool NetctlOptionsParser::ParsePacketCount(const std::string& text, uint32_t& out_count) {
    unsigned long long raw = std::stoull(text);
    if (raw > std::numeric_limits<uint32_t>::max()) {
        throw std::out_of_range("count out of range (0-4294967295)");
    }
    out_count = static_cast<uint32_t>(raw);
    return true;
}

NetctlOptions NetctlOptionsParser::Parse(int argc, char* argv[]) {
    NetctlOptions opts;

    if (argc < 2) {
        opts.command = NetctlCommand::Help;
        return opts;
    }

    std::string command = argv[1];
    std::string lower_cmd = command;
    std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);

    if (lower_cmd == "help" || lower_cmd == "--help" || lower_cmd == "-h" || lower_cmd == "/?") {
        opts.command = NetctlCommand::Help;
        return opts;
    }

    if (lower_cmd == "version" || lower_cmd == "--version" || lower_cmd == "-v" && argc == 2 || lower_cmd == "-V") {
        opts.command = NetctlCommand::Version;
        return opts;
    }

    if (lower_cmd == "list") {
        opts.command = NetctlCommand::List;
        return opts;
    }

    if (lower_cmd == "capture") {
        opts.command = NetctlCommand::Capture;
        try {
            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if ((arg == "-i" || arg == "--interface") && i + 1 < argc) {
                    opts.config.interface_ip = argv[++i];
                } else if ((arg == "-p" || arg == "--proto") && i + 1 < argc) {
                    opts.config.filter_protocol = argv[++i];
                    std::transform(opts.config.filter_protocol.begin(), opts.config.filter_protocol.end(), opts.config.filter_protocol.begin(), ::toupper);
                } else if (arg == "--port" && i + 1 < argc) {
                    ParsePortValue(argv[++i], opts.config.filter_port);
                } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                    opts.config.output_file = argv[++i];
                } else if (arg == "--pcap" && i + 1 < argc) {
                    opts.config.pcap_file = argv[++i];
                } else if ((arg == "-c" || arg == "--count") && i + 1 < argc) {
                    ParsePacketCount(argv[++i], opts.config.max_packets);
                } else if (arg == "--engine" && i + 1 < argc) {
                    std::string engine = argv[++i];
                    std::transform(engine.begin(), engine.end(), engine.begin(), ::tolower);
                    if (engine == "npcap") {
                        opts.config.use_npcap = true;
                    } else if (engine == "winsock") {
                        opts.config.use_npcap = false;
                    } else {
                        throw std::invalid_argument("engine must be winsock or npcap");
                    }
                } else if (arg == "-v" || arg == "--verbose") {
                    opts.config.verbose = true;
                } else if (arg == "-x" || arg == "--hex") {
                    opts.config.hex_dump = true;
                } else if (arg == "--no-color") {
                    opts.config.no_color = true;
                }
            }
        } catch (const std::exception& e) {
            opts.valid = false;
            opts.error_message = e.what();
            return opts;
        }

        if (opts.config.interface_ip.empty()) {
            opts.valid = false;
            opts.error_message = "Interface IP (-i) is required for capture command.";
            return opts;
        }

        return opts;
    }

    opts.command = NetctlCommand::Unknown;
    opts.unknown_command = command;
    return opts;
}

void NetctlOptionsParser::DisplayVersion() {
    std::cout << "netctl 2.0.0 (CrossShell for UNIX)\n";
}

void NetctlOptionsParser::DisplayHelp() {
    std::cout << Color::BOLD << Color::CYAN 
              << "\n"
              << " netctl -  Network Analyzer v2.0\n"
              << "\n" 
              << Color::RESET
              << Color::YELLOW << "USAGE:\n" << Color::RESET
              << "  netctl list\n"
              << "  netctl capture -i <INTERFACE_IP> [options]\n"
              << "  netctl help\n\n"
              << Color::YELLOW << "COMMANDS:\n" << Color::RESET
              << "  " << Color::GREEN << "list" << Color::RESET << "                  Display available active IPv4/IPv6 network adapters.\n"
              << "  " << Color::GREEN << "capture" << Color::RESET << "               Start asynchronous packet capture.\n"
              << "  " << Color::GREEN << "help" << Color::RESET << "                  Show this reference manual.\n\n"
              << Color::YELLOW << "CAPTURE OPTIONS:\n" << Color::RESET
              << "  " << Color::CYAN << "-i, --interface <IP>" << Color::RESET << "   Target local IPv4/IPv6 address (required).\n"
              << "  " << Color::CYAN << "-p, --proto <NAME>" << Color::RESET << "     Protocol filter: ALL, TCP, UDP, ICMP, DNS, HTTP.\n"
              << "  " << Color::CYAN << "--port <NUMBER>" << Color::RESET << "        Port filter range: 1..65535.\n"
              << "  " << Color::CYAN << "-o, --output <FILE>" << Color::RESET << "    Append plain-text output log to file.\n"
              << "  " << Color::CYAN << "--pcap <FILE>" << Color::RESET << "              Write binary PCAP capture to file.\n"
              << "  " << Color::CYAN << "--engine <winsock|npcap>" << Color::RESET << "  Capture backend (default: winsock).\n"
              << "  " << Color::CYAN << "-c, --count <NUM>" << Color::RESET << "      Stop after NUM matching packets (0 = unlimited).\n"
              << "  " << Color::CYAN << "-v, --verbose" << Color::RESET << "          Include extra TCP fields (Seq/Ack/Win).\n"
              << "  " << Color::CYAN << "-x, --hex" << Color::RESET << "              Include payload hex dump (truncated).\n"
              << "  " << Color::CYAN << "--no-color" << Color::RESET << "             Disable ANSI terminal colors.\n\n"

              << Color::YELLOW << "DEFAULTS:\n" << Color::RESET
              << "  Interface: required | Protocol: ALL | Port: none | Engine: winsock\n"
              << "  Packet count: unlimited | Color: enabled | Hex dump: disabled\n\n"

              << Color::YELLOW << "ENGINE DETAILS:\n" << Color::RESET
              << "  winsock  - Uses raw sockets (IP-level capture).\n"
              << "             IPv6 can be platform-dependent on Windows; recvfrom source address\n"
              << "             fallback is used when headerless payloads are returned.\n"
              << "             PCAP output link type: DLT_RAW (101).\n"
              << "  npcap    - Uses Npcap/WinPcap SDK (Ethernet Layer-2 capture with VLAN handling).\n"
              << "             Requires build with NETCTL_ENABLE_NPCAP and link against wpcap.lib\n"
              << "             and Packet.lib. PCAP output link type: DLT_EN10MB (1).\n\n"

              << Color::YELLOW << "FILTER NOTES:\n" << Color::RESET
              << "  --proto DNS maps to UDP/53 inspection.\n"
              << "  --proto HTTP maps to TCP request-line inspection (port 80/8080 heuristics).\n"
              << "  --port is applied in addition to protocol filtering when both are set.\n\n"

              << Color::YELLOW << "EXAMPLES:\n" << Color::RESET
              << "  netctl list\n"
              << "  netctl capture -i 192.168.1.10\n"
              << "  netctl capture -i fe80::abcd:1234%12 -p ICMP\n"
              << "  netctl capture -i 10.0.0.5 -p TCP --port 443 -v -c 200\n"
              << "  netctl capture -i 10.0.0.5 --pcap traffic.pcap\n"
              << "  netctl capture -i 10.0.0.5 --engine npcap --pcap l2_capture.pcap\n"
              << "  netctl capture -i 10.0.0.5 -o session.log --no-color\n\n"

              << Color::YELLOW << "EXIT BEHAVIOR:\n" << Color::RESET
              << "  Press Ctrl+C to stop capture gracefully.\n"
              << "  Capture also stops automatically when --count limit is reached.\n\n"

              << Color::RED << "PRIVILEGE NOTE: Administrator privileges are required for capture engines.\n"
              << "IPv6 link-local addresses must include a scope ID (example: fe80::1%12).\n"
              << "==============================================================================\n";
}
