// ============================================================================
// 4. SERVER ENGINE
// ============================================================================

class InetdServerEngine {
public:
    static bool ResolvePortOrService(const std::string& input, struct addrinfo* hints, struct addrinfo** result) {
        if (getaddrinfo(NULL, input.c_str(), hints, result) == 0) {
            return true;
        }

        bool is_numeric = !input.empty() && std::all_of(input.begin(), input.end(), [](unsigned char c) { return std::isdigit(c); });
        if (is_numeric) {
            hints->ai_flags |= AI_NUMERICSERV;
            return getaddrinfo(NULL, input.c_str(), hints, result) == 0;
        }
        return false;
    }

    static bool SetupSockets(std::vector<ServiceConfig>& services) {
        int bound_count = 0;
        for (auto& cfg : services) {
            struct addrinfo hints = {}, *result = nullptr;
            hints.ai_family   = AF_UNSPEC;
            hints.ai_socktype = (cfg.sock_type == "dgram") ? SOCK_DGRAM : SOCK_STREAM;
            hints.ai_protocol = (cfg.protocol == "udp") ? IPPROTO_UDP : IPPROTO_TCP;
            hints.ai_flags    = AI_PASSIVE;

            if (!ResolvePortOrService(cfg.port_str, &hints, &result) || result == nullptr) {
                InetdLogger::Instance().Log(LOG_ERROR, "Failed to resolve port/service for '" + cfg.name + "'");
                continue;
            }

            SOCKET listen_sock = INVALID_SOCKET;
            struct addrinfo* curr = result;
            while (curr != nullptr) {
                listen_sock = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
                if (listen_sock != INVALID_SOCKET) {
                    if (curr->ai_family == AF_INET6) {
                        int no = 0;
                        setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&no), sizeof(no));
                    }

                    BOOL reuse = TRUE;
                    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuse), sizeof(reuse));

                    if (bind(listen_sock, curr->ai_addr, static_cast<int>(curr->ai_addrlen)) != SOCKET_ERROR) {
                        break;
                    }
                    closesocket(listen_sock);
                    listen_sock = INVALID_SOCKET;
                }
                curr = curr->ai_next;
            }

            freeaddrinfo(result);

            if (listen_sock == INVALID_SOCKET) {
                InetdLogger::Instance().Log(LOG_ERROR, "Bind failed for service '" + cfg.name + "'. Winsock Error: " + std::to_string(WSAGetLastError()));
                continue;
            }

            if (cfg.sock_type == "stream") {
                if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
                    InetdLogger::Instance().Log(LOG_ERROR, "Listen failed for service '" + cfg.name + "'");
                    closesocket(listen_sock);
                    continue;
                }
            }

            cfg.listen_sock = listen_sock;
            bound_count++;
            InetdLogger::Instance().Log(LOG_INFO, "Bound listener on service '" + cfg.name + "' (Socket: " + std::to_string(listen_sock) + ")");
        }
        return bound_count > 0;
    }
};

