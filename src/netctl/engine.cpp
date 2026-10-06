#include "engine.hpp"

// --- Global Atomic State & Handles ---
static std::atomic<bool> g_running(true);
static SOCKET g_sniff_socket = INVALID_SOCKET;
static PacketQueue g_packet_queue;

// --- PcapWriter Implementation ---
PcapWriter::PcapWriter(const std::string& filename) {
    open(filename, 101);
}

bool PcapWriter::open(const std::string& filename, uint32_t network) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (filename.empty()) return false;
    file_.open(filename, std::ios::binary | std::ios::out);
    if (!file_.is_open()) return false;

    PcapGlobalHeader global_hdr;
    global_hdr.network = network;
    file_.write(reinterpret_cast<const char*>(&global_hdr), sizeof(global_hdr));
    return file_.good();
}

bool PcapWriter::is_open() const {
    return file_.is_open();
}

void PcapWriter::write_packet(const uint8_t* data, size_t length, const std::chrono::system_clock::time_point& tp) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open() || data == nullptr || length == 0) return;

    auto duration = tp.time_since_epoch();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>(duration - sec);

    PcapPacketHeader pkt_hdr{};
    pkt_hdr.ts_sec = static_cast<uint32_t>(sec.count());
    pkt_hdr.ts_usec = static_cast<uint32_t>(usec.count());
    pkt_hdr.incl_len = static_cast<uint32_t>(length);
    pkt_hdr.orig_len = static_cast<uint32_t>(length);

    file_.write(reinterpret_cast<const char*>(&pkt_hdr), sizeof(pkt_hdr));
    file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(length));
}

// --- PacketQueue Implementation ---
void PacketQueue::push(RawPacket&& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= max_capacity_) {
        queue_.pop(); // Drop oldest packet under extreme queue congestion
        dropped_count_++;
    }
    queue_.push(std::move(packet));
    cv_.notify_one();
}

bool PacketQueue::pop(RawPacket& packet, const std::atomic<bool>& running) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this, &running] { return !queue_.empty() || !running; });
    if (queue_.empty()) return false;
    packet = std::move(queue_.front());
    queue_.pop();
    return true;
}

void PacketQueue::notify_all() {
    cv_.notify_all();
}

uint64_t PacketQueue::get_dropped_count() const {
    return dropped_count_;
}

// --- Logger Implementation ---
Logger::Logger(const std::string& filepath, bool disable_color) : use_color_(!disable_color) {
    if (!filepath.empty()) {
        file_stream_.open(filepath, std::ios::out | std::ios::app);
        if (file_stream_.is_open()) {
            logging_to_file_ = true;
        } else {
            std::cerr << Color::RED << "[!] Warning: Could not open output file: " << filepath << Color::RESET << std::endl;
        }
    }
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }
}

void Logger::log(const std::string& colorized_msg, const std::string& plain_msg) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (use_color_) {
        std::cout << colorized_msg;
    } else {
        std::cout << plain_msg;
    }
    if (logging_to_file_) {
        file_stream_ << plain_msg;
        if (++write_count_ % 50 == 0) file_stream_.flush();
    }
}

// --- System & Utility Helpers ---
bool IsUserAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

void EnableVT100Colors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
}

void RequestStop() {
    g_running = false;
    if (g_sniff_socket != INVALID_SOCKET) {
        DWORD mode = RCVALL_OFF;
        DWORD bytesReturned = 0;
        WSAIoctl(g_sniff_socket, SIO_RCVALL, &mode, sizeof(mode), NULL, 0, &bytesReturned, NULL, NULL);
        closesocket(g_sniff_socket);
        g_sniff_socket = INVALID_SOCKET;
    }
    g_packet_queue.notify_all();
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        RequestStop();
        return TRUE;
    }
    return FALSE;
}

static std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(tp);
    std::tm bt;
    localtime_s(&bt, &timer);

    std::ostringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S") << '.' 
       << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

static std::string IPv4ToString(uint32_t ip) {
    in_addr addr;
    addr.s_addr = ip;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, buf, INET_ADDRSTRLEN);
    return std::string(buf);
}

static std::string IPv6ToString(const in6_addr& ip6) {
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ip6, buf, INET6_ADDRSTRLEN);
    return std::string(buf);
}

static bool ParseIPv6L4(const uint8_t* data, size_t bytes_recvd, uint8_t& protocol, size_t& ip_hdr_len) {
    if (data == nullptr || bytes_recvd < sizeof(IPv6Header)) return false;

    const IPv6Header* ip6 = reinterpret_cast<const IPv6Header*>(data);
    protocol = ip6->next_header;
    ip_hdr_len = sizeof(IPv6Header);

    constexpr size_t kMaxIPv6ExtensionHeaders = 8;
    for (size_t ext_count = 0; ext_count < kMaxIPv6ExtensionHeaders; ++ext_count) {
        if (protocol == IPPROTO_HOPOPTS || protocol == IPPROTO_ROUTING || protocol == IPPROTO_DSTOPTS) {
            if (bytes_recvd < ip_hdr_len + 2) return false;
            const uint8_t* ext = data + ip_hdr_len;
            uint8_t next = ext[0];
            size_t ext_len = static_cast<size_t>(ext[1] + 1) * 8;
            if (ext_len < 8 || bytes_recvd < ip_hdr_len + ext_len) return false;
            protocol = next;
            ip_hdr_len += ext_len;
            continue;
        }

        if (protocol == IPPROTO_FRAGMENT) {
            if (bytes_recvd < ip_hdr_len + 8) return false;
            const uint8_t* ext = data + ip_hdr_len;
            protocol = ext[0];
            ip_hdr_len += 8;
            continue;
        }

        if (protocol == IPPROTO_AH) {
            if (bytes_recvd < ip_hdr_len + 2) return false;
            const uint8_t* ext = data + ip_hdr_len;
            uint8_t next = ext[0];
            size_t ext_len = static_cast<size_t>(ext[1] + 2) * 4;
            if (ext_len < 8 || bytes_recvd < ip_hdr_len + ext_len) return false;
            protocol = next;
            ip_hdr_len += ext_len;
            continue;
        }

        return true;
    }

    return false;
}

static std::string SanitizePrintableLine(const uint8_t* payload, size_t inspect_len) {
    std::string line;
    line.reserve(inspect_len);
    for (size_t i = 0; i < inspect_len; ++i) {
        unsigned char c = payload[i];
        if (c == '\r' || c == '\n') break;
        if (std::isprint(c) && c != 0x1B) {
            line.push_back(static_cast<char>(c));
        } else {
            line.push_back('.');
        }
    }
    return line;
}

static std::string WideToUtf8(const wchar_t* wstr) {
    if (wstr == nullptr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return "";
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &out[0], len, nullptr, nullptr);
    out.pop_back();
    return out;
}

static bool ResolveCaptureAddress(const std::string& ip, sockaddr_storage& out_addr, int& out_family, int& out_len) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags = AI_NUMERICHOST;

    addrinfo* res = nullptr;
    if (getaddrinfo(ip.c_str(), nullptr, &hints, &res) != 0 || res == nullptr) {
        return false;
    }

    bool found = false;
    for (addrinfo* ptr = res; ptr != nullptr; ptr = ptr->ai_next) {
        if (ptr->ai_family == AF_INET || ptr->ai_family == AF_INET6) {
            std::memset(&out_addr, 0, sizeof(out_addr));
            std::memcpy(&out_addr, ptr->ai_addr, static_cast<size_t>(ptr->ai_addrlen));
            out_family = ptr->ai_family;
            out_len = static_cast<int>(ptr->ai_addrlen);
            found = true;
            break;
        }
    }

    freeaddrinfo(res);
    return found;
}

std::vector<std::pair<std::string, std::string>> ListNetworkInterfaces() {
    std::vector<std::pair<std::string, std::string>> interfaces;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG buffer_len = 0;
    DWORD status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &buffer_len);
    if (status != ERROR_BUFFER_OVERFLOW) {
        return interfaces;
    }

    std::vector<uint8_t> adapter_buffer(buffer_len);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(adapter_buffer.data());
    status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &buffer_len);
    if (status != NO_ERROR) {
        return interfaces;
    }

    for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) continue;
        std::string adapter_name = WideToUtf8(adapter->FriendlyName);
        if (adapter_name.empty()) adapter_name = "Adapter";

        for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr == nullptr) continue;

            int family = unicast->Address.lpSockaddr->sa_family;
            char ip_str[INET6_ADDRSTRLEN] = {};
            std::string ip_value;
            std::string label_suffix;

            if (family == AF_INET) {
                const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(unicast->Address.lpSockaddr);
                if (inet_ntop(AF_INET, &ipv4->sin_addr, ip_str, INET_ADDRSTRLEN) == nullptr) continue;
                ip_value = ip_str;
                label_suffix = " [IPv4]";
            } else if (family == AF_INET6) {
                const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(unicast->Address.lpSockaddr);
                if (inet_ntop(AF_INET6, &ipv6->sin6_addr, ip_str, INET6_ADDRSTRLEN) == nullptr) continue;
                ip_value = ip_str;
                if (ipv6->sin6_scope_id != 0 && IN6_IS_ADDR_LINKLOCAL(&ipv6->sin6_addr)) {
                    ip_value += "%" + std::to_string(ipv6->sin6_scope_id);
                }
                label_suffix = " [IPv6]";
            } else {
                continue;
            }

            interfaces.push_back({adapter_name + label_suffix, ip_value});
        }
    }

    return interfaces;
}

static std::string FormatHexDump(const uint8_t* buffer, size_t size) {
    std::ostringstream oss;
    for (size_t i = 0; i < size; i += 16) {
        oss << "  " << Color::GRAY << std::hex << std::setw(4) << std::setfill('0') << i << Color::RESET << "  ";
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i + j]) << " ";
            } else {
                oss << "   ";
            }
        }
        oss << " ";
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                char c = static_cast<char>(buffer[i + j]);
                oss << (std::isprint(static_cast<unsigned char>(c)) ? c : '.');
            }
        }
        oss << "\n";
    }
    return oss.str();
}

static std::string AnalyzeHTTPPayload(const uint8_t* payload, size_t length) {
    if (length == 0 || payload == nullptr) return "";
    size_t inspect_len = std::min(length, size_t(512));
    std::string data = SanitizePrintableLine(payload, inspect_len);

    if (data.rfind("GET ", 0) == 0 || data.rfind("POST ", 0) == 0 || 
        data.rfind("HTTP/1.", 0) == 0 || data.rfind("PUT ", 0) == 0 ||
        data.rfind("DELETE ", 0) == 0 || data.rfind("HEAD ", 0) == 0) {
        return data;
    }
    return "";
}

static std::string AnalyzeDNSPayload(const uint8_t* payload, size_t length) {
    if (length < sizeof(DNSHeader) || payload == nullptr) return "";
    const DNSHeader* dns = reinterpret_cast<const DNSHeader*>(payload);
    uint16_t flags = ntohs(dns->flags);
    uint16_t qdcount = ntohs(dns->qdcount);

    std::ostringstream oss;
    oss << "DNS " << ((flags & 0x8000) ? "Response" : "Query") 
        << " [TxID: 0x" << std::hex << ntohs(dns->id) << std::dec 
        << ", Qs: " << qdcount << "]";
    return oss.str();
}

static void PacketWorkerThread(const Config& config, Logger& logger) {
    RawPacket packet;
    uint32_t processed_count = 0;
    bool warned_ipv6_layout = false;
    PcapWriter pcap_writer;
    bool warned_pcap_open = false;

    if (!config.pcap_file.empty()) {
        if (!pcap_writer.open(config.pcap_file, 101)) {
            warned_pcap_open = true;
            std::cerr << Color::YELLOW << "[!] Warning: Could not open PCAP output file: "
                      << config.pcap_file << Color::RESET << std::endl;
        }
    }

    while (g_running) {
        if (!g_packet_queue.pop(packet, g_running)) {
            if (!g_running) break;
            continue;
        }

        const uint8_t* data = packet.data.data();
        size_t bytes_recvd = packet.data.size();

        std::string src_ip = "", dest_ip = "";
        uint8_t protocol = 0;
        size_t ip_hdr_len = 0;
        bool has_full_ip_header = true;

        if (packet.family == AF_INET) {
            if (bytes_recvd < sizeof(IPv4Header)) continue;
            if ((data[0] >> 4) != 4) continue;
            const IPv4Header* ip4 = reinterpret_cast<const IPv4Header*>(data);
            ip_hdr_len = (ip4->ver_ihl & 0x0F) * 4;
            if (ip_hdr_len < sizeof(IPv4Header) || bytes_recvd < ip_hdr_len) continue;

            src_ip = IPv4ToString(ip4->src_ip);
            dest_ip = IPv4ToString(ip4->dest_ip);
            protocol = ip4->protocol;
        } else if (packet.family == AF_INET6) {
            if (bytes_recvd == 0) continue;
            if ((data[0] >> 4) != 6) {
                has_full_ip_header = false;
                ip_hdr_len = 0;
                if (packet.src_addr.ss_family == AF_INET6) {
                    const auto* src6 = reinterpret_cast<const sockaddr_in6*>(&packet.src_addr);
                    src_ip = IPv6ToString(src6->sin6_addr);
                }
                dest_ip = config.interface_ip;

                if (!warned_ipv6_layout && !src_ip.empty()) {
                    std::cerr << Color::YELLOW
                              << "[!] Warning: IPv6 raw socket payload arrived without IPv6 header; "
                              << "using recvfrom source address for endpoint attribution."
                              << Color::RESET << std::endl;
                    warned_ipv6_layout = true;
                }
            } else {
                if (bytes_recvd < sizeof(IPv6Header)) continue;
                const IPv6Header* ip6 = reinterpret_cast<const IPv6Header*>(data);
                if (!ParseIPv6L4(data, bytes_recvd, protocol, ip_hdr_len)) continue;
                src_ip = IPv6ToString(ip6->src_ip);
                dest_ip = IPv6ToString(ip6->dest_ip);
            }
        } else {
            continue;
        }

        size_t l4_bytes_available = (bytes_recvd >= ip_hdr_len) ? (bytes_recvd - ip_hdr_len) : 0;
        const uint8_t* l4_data = data + ip_hdr_len;

        std::string proto_str = "OTHER";
        std::string proto_color = Color::WHITE;
        uint16_t src_port = 0, dest_port = 0;
        std::ostringstream detail_oss, detail_plain;
        const uint8_t* payload = nullptr;
        size_t payload_len = 0;
        bool match = false;

        if (packet.family == AF_INET6 && !has_full_ip_header) {
            if (config.filter_protocol == "TCP" || config.filter_protocol == "HTTP" || config.filter_protocol == "ALL") {
                if (l4_bytes_available >= sizeof(TCPHeader)) {
                    const TCPHeader* tcp = reinterpret_cast<const TCPHeader*>(l4_data);
                    size_t tcp_hdr_len = ((tcp->data_offset_reserved >> 4) & 0x0F) * 4;
                    if (tcp_hdr_len >= sizeof(TCPHeader) && l4_bytes_available >= tcp_hdr_len) {
                        proto_str = "TCP";
                        proto_color = Color::GREEN;
                        src_port = ntohs(tcp->src_port);
                        dest_port = ntohs(tcp->dest_port);
                        payload = l4_data + tcp_hdr_len;
                        payload_len = l4_bytes_available - tcp_hdr_len;
                    }
                }
            }

            if (proto_str == "OTHER" && (config.filter_protocol == "UDP" || config.filter_protocol == "DNS" || config.filter_protocol == "ALL")) {
                if (l4_bytes_available >= sizeof(UDPHeader)) {
                    const UDPHeader* udp = reinterpret_cast<const UDPHeader*>(l4_data);
                    proto_str = "UDP";
                    proto_color = Color::YELLOW;
                    src_port = ntohs(udp->src_port);
                    dest_port = ntohs(udp->dest_port);
                    payload = l4_data + sizeof(UDPHeader);
                    payload_len = l4_bytes_available - sizeof(UDPHeader);

                    if (src_port == 53 || dest_port == 53) {
                        proto_str = "DNS";
                        proto_color = Color::BLUE;
                        std::string dns_info = AnalyzeDNSPayload(payload, payload_len);
                        if (!dns_info.empty()) {
                            detail_oss << " (" << dns_info << ")";
                            detail_plain << " (" << dns_info << ")";
                        }
                    }
                }
            }
        }
        else if (protocol == IPPROTO_TCP && l4_bytes_available >= sizeof(TCPHeader)) {
            proto_str = "TCP";
            proto_color = Color::GREEN;
            const TCPHeader* tcp = reinterpret_cast<const TCPHeader*>(l4_data);
            src_port = ntohs(tcp->src_port);
            dest_port = ntohs(tcp->dest_port);

            size_t tcp_hdr_len = ((tcp->data_offset_reserved >> 4) & 0x0F) * 4;
            if (tcp_hdr_len >= sizeof(TCPHeader) && l4_bytes_available >= tcp_hdr_len) {
                payload = l4_data + tcp_hdr_len;
                payload_len = l4_bytes_available - tcp_hdr_len;
            }

            if (src_port == 80 || dest_port == 80 || src_port == 8080 || dest_port == 8080) {
                std::string http_info = AnalyzeHTTPPayload(payload, payload_len);
                if (!http_info.empty()) {
                    proto_str = "HTTP";
                    proto_color = Color::CYAN;
                    detail_oss << " (" << http_info << ")";
                    detail_plain << " (" << http_info << ")";
                }
            } else if (src_port == 443 || dest_port == 443) {
                proto_str = "TLS/HTTPS";
                proto_color = Color::MAGENTA;
            }

            if (config.verbose) {
                detail_oss << " [Seq: " << ntohl(tcp->seq_num) << " Ack: " << ntohl(tcp->ack_num) 
                           << " Win: " << ntohs(tcp->window_size) << "]";
                detail_plain << " [Seq: " << ntohl(tcp->seq_num) << " Ack: " << ntohl(tcp->ack_num) 
                             << " Win: " << ntohs(tcp->window_size) << "]";
            }
        } 
        else if (protocol == IPPROTO_UDP && l4_bytes_available >= sizeof(UDPHeader)) {
            proto_str = "UDP";
            proto_color = Color::YELLOW;
            const UDPHeader* udp = reinterpret_cast<const UDPHeader*>(l4_data);
            src_port = ntohs(udp->src_port);
            dest_port = ntohs(udp->dest_port);

            if (l4_bytes_available >= sizeof(UDPHeader)) {
                payload = l4_data + sizeof(UDPHeader);
                payload_len = l4_bytes_available - sizeof(UDPHeader);
            }

            if (src_port == 53 || dest_port == 53) {
                proto_str = "DNS";
                proto_color = Color::BLUE;
                std::string dns_info = AnalyzeDNSPayload(payload, payload_len);
                if (!dns_info.empty()) {
                    detail_oss << " (" << dns_info << ")";
                    detail_plain << " (" << dns_info << ")";
                }
            }
        } 
        else if ((protocol == IPPROTO_ICMP || protocol == IPPROTO_ICMPV6) && l4_bytes_available >= sizeof(ICMPHeader)) {
            proto_str = (protocol == IPPROTO_ICMP) ? "ICMP" : "ICMPv6";
            proto_color = Color::RED;
            const ICMPHeader* icmp = reinterpret_cast<const ICMPHeader*>(l4_data);
            detail_oss << " [Type: " << (int)icmp->type << " Code: " << (int)icmp->code << "]";
            detail_plain << " [Type: " << (int)icmp->type << " Code: " << (int)icmp->code << "]";
        }

        if (config.filter_protocol == "ALL") match = true;
        else if (config.filter_protocol == "TCP" && (proto_str == "TCP" || proto_str == "HTTP" || proto_str == "TLS/HTTPS")) match = true;
        else if (config.filter_protocol == "UDP" && (proto_str == "UDP" || proto_str == "DNS")) match = true;
        else if (config.filter_protocol == "ICMP" && (proto_str == "ICMP" || proto_str == "ICMPv6")) match = true;
        else if (config.filter_protocol == "DNS" && proto_str == "DNS") match = true;
        else if (config.filter_protocol == "HTTP" && proto_str == "HTTP") match = true;

        if (config.filter_port != 0 && src_port != config.filter_port && dest_port != config.filter_port) {
            match = false;
        }

        if (!match) continue;

        processed_count++;
        std::string ts = FormatTimestamp(packet.timestamp);

        std::ostringstream line_color, line_plain;

        line_color << Color::GRAY << "[" << ts << "] " << Color::RESET
                   << proto_color << Color::BOLD << "[" << std::left << std::setw(9) << proto_str << "] " << Color::RESET
                   << Color::WHITE << src_ip << Color::RESET;
        if (src_port) line_color << ":" << Color::YELLOW << src_port << Color::RESET;
        
        line_color << " -> " << Color::WHITE << dest_ip << Color::RESET;
        if (dest_port) line_color << ":" << Color::YELLOW << dest_port << Color::RESET;

        line_color << " | " << Color::GRAY << "Len: " << bytes_recvd << Color::RESET
                   << detail_oss.str() << "\n";

        line_plain << "[" << ts << "] "
                   << "[" << std::left << std::setw(9) << proto_str << "] "
                   << src_ip;
        if (src_port) line_plain << ":" << src_port;
        line_plain << " -> " << dest_ip;
        if (dest_port) line_plain << ":" << dest_port;
        line_plain << " | Len: " << bytes_recvd 
                   << detail_plain.str() << "\n";

        if (config.hex_dump && payload && payload_len > 0) {
            std::string dump = FormatHexDump(payload, std::min(payload_len, size_t(128)));
            line_color << dump;
            line_plain << dump;
        }

        logger.log(line_color.str(), line_plain.str());

        if (!config.pcap_file.empty() && !warned_pcap_open && pcap_writer.is_open()) {
            pcap_writer.write_packet(data, bytes_recvd, packet.timestamp);
        }

        if (config.max_packets > 0 && processed_count >= config.max_packets) {
            std::cout << Color::YELLOW << "\n[*] Packet limit (" << config.max_packets << ") reached. Stopping..." << Color::RESET << std::endl;
            g_running = false;
            break;
        }
    }
}

#ifdef NETCTL_ENABLE_NPCAP
static std::chrono::system_clock::time_point TimePointFromUnixUsec(long sec, long usec) {
    auto s = std::chrono::seconds(static_cast<long long>(sec));
    auto us = std::chrono::microseconds(static_cast<long long>(usec));
    return std::chrono::system_clock::time_point(s + us);
}

static std::string BuildBpfFilter(const Config& config) {
    std::string expr;

    if (config.filter_protocol == "TCP") expr = "tcp";
    else if (config.filter_protocol == "UDP") expr = "udp";
    else if (config.filter_protocol == "ICMP") expr = "icmp or icmp6";
    else if (config.filter_protocol == "DNS") expr = "udp port 53";
    else if (config.filter_protocol == "HTTP") expr = "tcp port 80 or tcp port 8080";

    if (config.filter_port != 0) {
        std::string port_expr = "port " + std::to_string(config.filter_port);
        if (expr.empty()) expr = port_expr;
        else expr = "(" + expr + ") and " + port_expr;
    }

    return expr;
}

static bool NpcapAddressMatches(const sockaddr* addr, const sockaddr_storage& wanted) {
    if (addr == nullptr) return false;
    if (addr->sa_family != wanted.ss_family) return false;

    if (addr->sa_family == AF_INET) {
        const auto* a = reinterpret_cast<const sockaddr_in*>(addr);
        const auto* w = reinterpret_cast<const sockaddr_in*>(&wanted);
        return a->sin_addr.s_addr == w->sin_addr.s_addr;
    }

    if (addr->sa_family == AF_INET6) {
        const auto* a = reinterpret_cast<const sockaddr_in6*>(addr);
        const auto* w = reinterpret_cast<const sockaddr_in6*>(&wanted);
        if (std::memcmp(&a->sin6_addr, &w->sin6_addr, sizeof(in6_addr)) != 0) return false;
        if (w->sin6_scope_id != 0 && a->sin6_scope_id != w->sin6_scope_id) return false;
        return true;
    }

    return false;
}

static bool FindNpcapDeviceByIp(const std::string& interface_ip, std::string& out_device_name, std::string& out_desc) {
    sockaddr_storage wanted{};
    int wanted_family = AF_UNSPEC;
    int wanted_len = 0;
    if (!ResolveCaptureAddress(interface_ip, wanted, wanted_family, wanted_len)) {
        (void)wanted_len;
        return false;
    }

    char errbuf[PCAP_ERRBUF_SIZE] = {};
    pcap_if_t* alldevs = nullptr;
    if (pcap_findalldevs(&alldevs, errbuf) == -1 || alldevs == nullptr) {
        return false;
    }

    bool found = false;
    for (pcap_if_t* dev = alldevs; dev != nullptr && !found; dev = dev->next) {
        for (pcap_addr_t* a = dev->addresses; a != nullptr; a = a->next) {
            if (NpcapAddressMatches(a->addr, wanted)) {
                out_device_name = dev->name ? dev->name : "";
                out_desc = dev->description ? dev->description : out_device_name;
                found = !out_device_name.empty();
                break;
            }
        }
    }

    pcap_freealldevs(alldevs);
    return found;
}

void StartNpcapCapture(const Config& config, Logger& logger) {
    std::string device_name;
    std::string device_desc;
    if (!FindNpcapDeviceByIp(config.interface_ip, device_name, device_desc)) {
        std::cerr << Color::RED << "[!] Failed to map interface IP to an Npcap device: "
                  << config.interface_ip << Color::RESET << std::endl;
        return;
    }

    char errbuf[PCAP_ERRBUF_SIZE] = {};
    pcap_t* handle = pcap_open_live(device_name.c_str(), 65536, 1, 500, errbuf);
    if (handle == nullptr) {
        std::cerr << Color::RED << "[!] Failed to open Npcap interface: " << errbuf << Color::RESET << std::endl;
        return;
    }

    std::string bpf_expr = BuildBpfFilter(config);
    if (!bpf_expr.empty()) {
        bpf_program program{};
        if (pcap_compile(handle, &program, bpf_expr.c_str(), 1, PCAP_NETMASK_UNKNOWN) != -1) {
            if (pcap_setfilter(handle, &program) == -1) {
                std::cerr << Color::YELLOW << "[!] Warning: Failed to apply Npcap filter: "
                          << pcap_geterr(handle) << Color::RESET << std::endl;
            }
            pcap_freecode(&program);
        } else {
            std::cerr << Color::YELLOW << "[!] Warning: Failed to compile Npcap filter '"
                      << bpf_expr << "': " << pcap_geterr(handle) << Color::RESET << std::endl;
        }
    }

    std::ostringstream header_oss, header_plain;
    header_oss << Color::BOLD << Color::GREEN
               << "[+] Npcap Layer-2 capture active on " << device_desc
               << " | Filter: " << config.filter_protocol
               << (config.filter_port ? (" | Port: " + std::to_string(config.filter_port)) : "")
               << "\n[+] Async Producer-Consumer Engine active. Press Ctrl+C to stop...\n" << Color::RESET
               << "------------------------------------------------------------------------------\n";

    header_plain << "[+] Npcap Layer-2 capture active on " << device_desc
                 << " | Filter: " << config.filter_protocol
                 << (config.filter_port ? (" | Port: " + std::to_string(config.filter_port)) : "")
                 << "\n[+] Async Producer-Consumer Engine active. Press Ctrl+C to stop...\n"
                 << "------------------------------------------------------------------------------\n";

    logger.log(header_oss.str(), header_plain.str());

    Config worker_config = config;
    worker_config.pcap_file.clear();
    std::thread worker(PacketWorkerThread, std::cref(worker_config), std::ref(logger));

    PcapWriter l2_pcap_writer;
    bool warned_pcap_open = false;
    if (!config.pcap_file.empty() && !l2_pcap_writer.open(config.pcap_file, 1)) {
        warned_pcap_open = true;
        std::cerr << Color::YELLOW << "[!] Warning: Could not open PCAP output file: "
                  << config.pcap_file << Color::RESET << std::endl;
    }

    struct EthernetHeader {
        uint8_t dest[6];
        uint8_t src[6];
        uint16_t ether_type;
    };

    while (g_running) {
        pcap_pkthdr* hdr = nullptr;
        const u_char* pkt_data = nullptr;
        int res = pcap_next_ex(handle, &hdr, &pkt_data);
        if (res == 0) continue;
        if (res < 0) {
            if (g_running) {
                std::cerr << Color::YELLOW << "[!] Npcap capture loop ended: "
                          << pcap_geterr(handle) << Color::RESET << std::endl;
            }
            g_running = false;
            break;
        }
        if (hdr == nullptr || pkt_data == nullptr || hdr->caplen == 0) continue;

        auto ts = TimePointFromUnixUsec(hdr->ts.tv_sec, hdr->ts.tv_usec);
        if (!config.pcap_file.empty() && !warned_pcap_open && l2_pcap_writer.is_open()) {
            l2_pcap_writer.write_packet(pkt_data, hdr->caplen, ts);
        }

        if (hdr->caplen < sizeof(EthernetHeader)) continue;
        const auto* eth = reinterpret_cast<const EthernetHeader*>(pkt_data);
        uint16_t ether_type = ntohs(eth->ether_type);
        size_t offset = sizeof(EthernetHeader);

        while ((ether_type == 0x8100 || ether_type == 0x88A8) && hdr->caplen >= offset + 4) {
            const uint8_t* vlan = pkt_data + offset;
            ether_type = static_cast<uint16_t>((static_cast<uint16_t>(vlan[2]) << 8) | vlan[3]);
            offset += 4;
        }

        int family = AF_UNSPEC;
        if (ether_type == 0x0800) family = AF_INET;
        else if (ether_type == 0x86DD) family = AF_INET6;
        else continue;

        if (hdr->caplen <= offset) continue;
        size_t ip_len = hdr->caplen - offset;

        RawPacket packet;
        packet.data.resize(ip_len);
        std::memcpy(packet.data.data(), pkt_data + offset, ip_len);
        packet.timestamp = ts;
        packet.family = family;
        std::memset(&packet.src_addr, 0, sizeof(packet.src_addr));

        g_packet_queue.push(std::move(packet));
    }

    pcap_close(handle);
    g_packet_queue.notify_all();
    if (worker.joinable()) worker.join();

    uint64_t drops = g_packet_queue.get_dropped_count();
    if (drops > 0) {
        std::cout << Color::RED << "[!] Warning: Queue congestion resulted in " << drops << " dropped packets." << Color::RESET << std::endl;
    }
}
#else
void StartNpcapCapture(const Config& config, Logger& logger) {
    (void)config;
    (void)logger;
    std::cerr << Color::YELLOW
              << "[!] Npcap engine requested but this build does not include Npcap support. "
              << "Rebuild with NETCTL_ENABLE_NPCAP and link wpcap.lib/Packet.lib."
              << Color::RESET << std::endl;
}
#endif

void StartCaptureEngine(const Config& config, Logger& logger) {
    sockaddr_storage bind_addr{};
    int bind_len = 0;
    int family = AF_UNSPEC;
    if (!ResolveCaptureAddress(config.interface_ip, bind_addr, family, bind_len)) {
        std::cerr << Color::RED << "[!] Invalid interface IP: " << config.interface_ip << "\n"
                  << "    For link-local IPv6, include scope (example: fe80::1%12)."
                  << Color::RESET << std::endl;
        return;
    }

    SOCKET sniffer = socket(family, SOCK_RAW, (family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6);
    if (sniffer == INVALID_SOCKET) {
        std::cerr << Color::RED << "[!] Failed to create raw socket. Error: " << WSAGetLastError() 
                  << "\n    Make sure you are running netctl as Administrator!" << Color::RESET << std::endl;
        return;
    }
    g_sniff_socket = sniffer;

    if (bind(sniffer, reinterpret_cast<sockaddr*>(&bind_addr), bind_len) == SOCKET_ERROR) {
        std::cerr << Color::RED << "[!] Failed to bind raw socket to " << config.interface_ip
                  << ". Error: " << WSAGetLastError() << Color::RESET << std::endl;
        closesocket(sniffer);
        return;
    }

    DWORD in_val = RCVALL_ON;
    DWORD bytes_returned = 0;
    if (WSAIoctl(sniffer, SIO_RCVALL, &in_val, sizeof(in_val), NULL, 0, &bytes_returned, NULL, NULL) == SOCKET_ERROR) {
        std::cerr << Color::RED << "[!] SIO_RCVALL promiscuous mode failed. Error: " << WSAGetLastError() << Color::RESET << std::endl;
        closesocket(sniffer);
        return;
    }

    DWORD timeout_ms = 500;
    if (setsockopt(sniffer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms)) == SOCKET_ERROR) {
        std::cerr << Color::YELLOW << "[!] Warning: Failed to set receive timeout. Error: "
                  << WSAGetLastError() << Color::RESET << std::endl;
    }

    int rcvbuf_size = 16 * 1024 * 1024;
    if (setsockopt(sniffer, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf_size), sizeof(rcvbuf_size)) == SOCKET_ERROR) {
        std::cerr << Color::YELLOW << "[!] Warning: Failed to set SO_RCVBUF size. Error: "
                  << WSAGetLastError() << Color::RESET << std::endl;
    }

    std::ostringstream header_oss, header_plain;
    header_oss << Color::BOLD << Color::GREEN 
               << "[+] Promiscuous capture active on " << config.interface_ip 
               << " | Filter: " << config.filter_protocol 
               << (config.filter_port ? (" | Port: " + std::to_string(config.filter_port)) : "")
               << "\n[+] Async Producer-Consumer Engine active. Press Ctrl+C to stop...\n" << Color::RESET
               << "------------------------------------------------------------------------------\n";
    
    header_plain << "[+] Promiscuous capture active on " << config.interface_ip 
                 << " | Filter: " << config.filter_protocol 
                 << (config.filter_port ? (" | Port: " + std::to_string(config.filter_port)) : "")
                 << "\n[+] Async Producer-Consumer Engine active. Press Ctrl+C to stop...\n"
                 << "------------------------------------------------------------------------------\n";

    if (family == AF_INET6) {
        header_oss << Color::YELLOW
                   << "[!] Note: Native Winsock IPv6 raw capture has platform limitations and may not expose all packet forms.\n"
                   << Color::RESET;
        header_plain << "[!] Note: Native Winsock IPv6 raw capture has platform limitations and may not expose all packet forms.\n";
    }

    logger.log(header_oss.str(), header_plain.str());

    std::thread worker(PacketWorkerThread, std::cref(config), std::ref(logger));

    std::vector<uint8_t> buffer(65536);
    RawPacket packet;
    packet.data.reserve(buffer.size());

    while (g_running) {
        sockaddr_storage src_addr{};
        int src_len = sizeof(src_addr);
        int bytes_recvd = recvfrom(
            sniffer,
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&src_addr),
            &src_len
        );
        if (bytes_recvd == SOCKET_ERROR) {
            int wsa_error = WSAGetLastError();
            if (wsa_error == WSAETIMEDOUT || wsa_error == WSAEWOULDBLOCK) {
                continue;
            }
            if (!g_running) break;
            continue;
        }

        if (bytes_recvd <= 0) {
            if (!g_running) break;
            continue;
        }

        packet.data.resize(static_cast<size_t>(bytes_recvd));
        std::memcpy(packet.data.data(), buffer.data(), static_cast<size_t>(bytes_recvd));
        packet.timestamp = std::chrono::system_clock::now();
        packet.family = family;
        packet.src_addr = src_addr;

        g_packet_queue.push(std::move(packet));
        if (packet.data.capacity() < buffer.size()) {
            packet.data.reserve(buffer.size());
        }
    }

    g_packet_queue.notify_all();
    if (worker.joinable()) worker.join();

    uint64_t drops = g_packet_queue.get_dropped_count();
    if (drops > 0) {
        std::cout << Color::RED << "[!] Warning: Queue congestion resulted in " << drops << " dropped packets." << Color::RESET << std::endl;
    }

    if (sniffer != INVALID_SOCKET) {
        closesocket(sniffer);
        g_sniff_socket = INVALID_SOCKET;
    }
}
