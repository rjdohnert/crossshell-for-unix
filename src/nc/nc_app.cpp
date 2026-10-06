#include "nc_app.hpp"

int NetcatApplication::Run(int argc, char* argv[]) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    WinsockScope winsock;
    if (!winsock.IsInitialized()) return 1;

    NcOptions opt;
    if (!opt.Parse(argc, argv)) {
        return 1;
    }

    if (opt.showHelp) {
        opt.PrintHelp();
        return 0;
    }

    if (opt.showVersion) {
        opt.PrintVersion();
        return 0;
    }

    if (!opt.listen && (opt.destination.empty() || opt.ports.empty())) {
        std::cerr << "nc: missing destination host or port\n";
        return 1;
    }

    // SERVER / LISTEN MODE
    if (opt.listen) {
        struct addrinfo hints = {}, *res = NULL;
        hints.ai_family = opt.ipv4_only ? AF_INET : (opt.ipv6_only ? AF_INET6 : AF_UNSPEC);
        hints.ai_socktype = opt.udp ? SOCK_DGRAM : SOCK_STREAM;
        hints.ai_protocol = opt.udp ? IPPROTO_UDP : IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        std::string port_str = std::to_string(opt.local_port);
        if (getaddrinfo(opt.source_ip.empty() ? NULL : opt.source_ip.c_str(), port_str.c_str(), &hints, &res) != 0) {
            return 1;
        }

        SOCKET listen_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        int optval = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
        bind(listen_sock, res->ai_addr, (int)res->ai_addrlen);
        freeaddrinfo(res);

        if (!opt.udp) listen(listen_sock, SOMAXCONN);

        do {
            if (opt.verbose) std::cout << "listening on [" << (opt.source_ip.empty() ? "0.0.0.0" : opt.source_ip) << "] " << opt.local_port << " ...\n";

            SOCKET client_sock = INVALID_SOCKET;
            sockaddr_storage client_addr = {};
            int client_len = sizeof(client_addr);

            if (opt.udp) {
                char dummy[1];
                int n = recvfrom(listen_sock, dummy, sizeof(dummy), MSG_PEEK, (sockaddr*)&client_addr, &client_len);
                if (n == SOCKET_ERROR) break;

                connect(listen_sock, (sockaddr*)&client_addr, client_len);
                client_sock = listen_sock;
            } else {
                client_sock = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
            }

            g_active_socket.store(client_sock);

            if (!opt.exec_cmd.empty()) NetcatPipeline::ExecuteCommand(client_sock, opt.exec_cmd);
            else if (!opt.zero_io) NetcatPipeline::RelayIo(client_sock, opt);

            if (opt.udp) {
                sockaddr_storage unspec = {};
                unspec.ss_family = AF_UNSPEC;
                connect(listen_sock, (sockaddr*)&unspec, sizeof(unspec));
            } else {
                closesocket(client_sock);
            }

            g_active_socket.store(INVALID_SOCKET);

        } while (opt.keep_open);

        closesocket(listen_sock);
        return 0;
    }

    // CLIENT MODE
    int exit_code = 0;
    for (int port : opt.ports) {
        struct addrinfo hints = {}, *res = NULL;
        hints.ai_family = opt.ipv4_only ? AF_INET : (opt.ipv6_only ? AF_INET6 : AF_UNSPEC);
        hints.ai_socktype = opt.udp ? SOCK_DGRAM : SOCK_STREAM;
        hints.ai_protocol = opt.udp ? IPPROTO_UDP : IPPROTO_TCP;

        std::string port_str = std::to_string(port);
        if (getaddrinfo(opt.destination.c_str(), port_str.c_str(), &hints, &res) != 0) continue;

        SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        g_active_socket.store(sock);

        if (!opt.source_ip.empty() || opt.local_port != 0) {
            struct addrinfo bind_hints = {}, *bind_res = NULL;
            bind_hints.ai_family = res->ai_family;
            bind_hints.ai_socktype = res->ai_socktype;
            bind_hints.ai_protocol = res->ai_protocol;
            bind_hints.ai_flags = AI_PASSIVE;

            std::string local_port_str = std::to_string(opt.local_port);
            const char* local_ip = opt.source_ip.empty() ? NULL : opt.source_ip.c_str();

            if (getaddrinfo(local_ip, local_port_str.c_str(), &bind_hints, &bind_res) == 0) {
                int optval = 1;
                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
                bind(sock, bind_res->ai_addr, (int)bind_res->ai_addrlen);
                freeaddrinfo(bind_res);
            }
        }

        bool connected = false;

        if (opt.udp) {
            BOOL bNewBehavior = TRUE;
            DWORD dwBytesReturned = 0;
            WSAIoctl(sock, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);

            if (connect(sock, res->ai_addr, (int)res->ai_addrlen) == 0) {
                connected = true;
            }

            if (opt.zero_io) {
                send(sock, "", 0, 0);

                fd_set readfds, exceptfds;
                FD_ZERO(&readfds);
                FD_ZERO(&exceptfds);
                FD_SET(sock, &readfds);
                FD_SET(sock, &exceptfds);

                timeval tv = { opt.timeout > 0 ? opt.timeout : 1, 0 };

                if (select(0, &readfds, NULL, &exceptfds, &tv) > 0) {
                    char dummy[1];
                    if (recv(sock, dummy, sizeof(dummy), 0) == SOCKET_ERROR) {
                        int err = WSAGetLastError();
                        if (err == WSAECONNRESET || err == WSAECONNREFUSED) {
                            connected = false;
                        }
                    }
                }
            }
        } else {
            connected = NetcatPipeline::ConnectWithTimeout(sock, res->ai_addr, (int)res->ai_addrlen, opt.timeout);
        }

        if (connected) {
            if (opt.verbose || opt.zero_io) {
                std::cout << "Connection to " << opt.destination << " " << port << " port [" << (opt.udp ? "udp" : "tcp") << "] succeeded!\n";
            }
            if (!opt.zero_io) {
                if (!opt.exec_cmd.empty()) NetcatPipeline::ExecuteCommand(sock, opt.exec_cmd);
                else NetcatPipeline::RelayIo(sock, opt);
            }
        } else {
            if (opt.verbose) std::cerr << "nc: connect to " << opt.destination << " port " << port << " failed\n";
            exit_code = 1;
        }

        closesocket(sock);
        g_active_socket.store(INVALID_SOCKET);
        freeaddrinfo(res);
    }

    return exit_code;
}
