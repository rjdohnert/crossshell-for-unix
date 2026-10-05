// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

static std::atomic<bool> g_running(true);

static BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        InetdLogger::Instance().Log(LOG_INFO, "Shutdown signal received. Terminating inetd...");
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

class InetdOptions {
public:
    std::string config_path = "./inetd.conf";
    bool debug = false;
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "-d" || arg == "--debug") {
                debug = true;
                verbose = true;
            } else if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
                config_path = argv[++i];
            }
        }
        return true;
    }

    void PrintHelp(const char* prog_name) const {
        std::cout << R"(inetd(1)                CrossShell for UNIX Reference Manual                 inetd(1)

    NAME
        inetd - internet super-server daemon for Windows

    SYNOPSIS
        inetd [OPTIONS] [CONFIG_FILE]

    DESCRIPTION
        inetd listens on configured internet sockets and spawns corresponding
        server programs or handlers when connection requests arrive.

    OPTIONS
        -c, --config FILE
            Specify alternative configuration file path.

        -d, --debug
            Enable debugging output and run in foreground.

        -v, --verbose
            Enable verbose execution logging.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        inetd -d -c inetd.conf
            Run inetd in foreground debug mode with custom config.

    CrossShell for UNIX                                                  inetd(1)
)";
    }

    void PrintVersion() const {
        std::cout << "inetd 1.0.0\n";
    }
};

class InetdApplication {
public:
    int Run(int argc, char* argv[]) {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        InetdOptions opt;
        if (!opt.Parse(argc, argv)) {
            return 1;
        }

        if (opt.showHelp) {
            opt.PrintHelp("inetd.exe");
            return 0;
        }

        if (opt.showVersion) {
            opt.PrintVersion();
            return 0;
        }

        InetdLogger::Instance().SetDebug(opt.debug);
        InetdLogger::Instance().SetVerbose(opt.verbose);

        if (!AdminPrivilegeChecker::IsRunningAsAdmin()) {
            std::cerr << "[ERROR] Administrator privileges are required to run inetd.\n";
            std::cerr << "Please re-run the application from an elevated terminal session.\n";
            return 1;
        }

        InetdLogger::Instance().Log(LOG_INFO, "Privileges verified: Running as Administrator.");

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            InetdLogger::Instance().Log(LOG_ERROR, "WSAStartup initialization failed.");
            return 1;
        }

        SetConsoleCtrlHandler(ConsoleHandler, TRUE);

        std::vector<ServiceConfig> services;
        if (!InetdConfigParser::Parse(opt.config_path, services)) {
            InetdLogger::Instance().Log(LOG_ERROR, "Failed to parse configuration or file is empty.");
            return 1;
        }

        if (!InetdServerEngine::SetupSockets(services)) {
            InetdLogger::Instance().Log(LOG_ERROR, "Failed to bind listening sockets.");
            return 1;
        }

        InetdLogger::Instance().Log(LOG_INFO, "inetd initialization complete. Listening for connections...");

        while (g_running) {
            fd_set readfds;
            FD_ZERO(&readfds);
            SOCKET max_fd = 0;

            for (const auto& service : services) {
                if (service.listen_sock != INVALID_SOCKET && !service.is_busy->load()) {
                    FD_SET(service.listen_sock, &readfds);
                    if (service.listen_sock > max_fd) {
                        max_fd = service.listen_sock;
                    }
                }
            }

            if (readfds.fd_count == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                continue;
            }

            timeval timeout = {};
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int activity = select(static_cast<int>(max_fd + 1), &readfds, NULL, NULL, &timeout);

            if (activity == SOCKET_ERROR) {
                if (!g_running) break;
                InetdLogger::Instance().Log(LOG_ERROR, "Select loop error: " + std::to_string(WSAGetLastError()));
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            if (activity > 0) {
                for (auto& service : services) {
                    if (service.listen_sock != INVALID_SOCKET && FD_ISSET(service.listen_sock, &readfds)) {
                        if (service.sock_type == "stream") {
                            sockaddr_storage client_addr = {};
                            int addr_len = sizeof(client_addr);
                            SOCKET client_sock = accept(service.listen_sock, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

                            if (client_sock == INVALID_SOCKET) continue;

                            char ip_str[INET6_ADDRSTRLEN] = {0};
                            if (client_addr.ss_family == AF_INET) {
                                sockaddr_in* s4 = reinterpret_cast<sockaddr_in*>(&client_addr);
                                inet_ntop(AF_INET, &(s4->sin_addr), ip_str, sizeof(ip_str));
                                InetdLogger::Instance().Log(LOG_DEBUG, "Connection accepted on '" + service.name + "' (TCP) from " +
                                               std::string(ip_str) + ":" + std::to_string(ntohs(s4->sin_port)));
                            } else if (client_addr.ss_family == AF_INET6) {
                                sockaddr_in6* s6 = reinterpret_cast<sockaddr_in6*>(&client_addr);
                                inet_ntop(AF_INET6, &(s6->sin6_addr), ip_str, sizeof(ip_str));
                                InetdLogger::Instance().Log(LOG_DEBUG, "Connection accepted on '" + service.name + "' (TCP) from [" +
                                               std::string(ip_str) + "]:" + std::to_string(ntohs(s6->sin6_port)));
                            }

                            if (service.wait_flag) {
                                service.is_busy->store(true);
                            }

                            auto busy_ptr = service.is_busy;
                            bool wait_flag = service.wait_flag;
                            std::string prog = service.program;
                            std::string s_name = service.name;

                            try {
                                std::thread([client_sock, service, busy_ptr, wait_flag, prog, s_name]() {
                                    if (prog == "internal") {
                                        BuiltinServicesEngine::HandleTCP(client_sock, s_name);
                                    } else {
                                        ProcessLauncher::Launch(client_sock, service);
                                    }
                                    if (wait_flag) {
                                        busy_ptr->store(false);
                                    }
                                }).detach();
                            } catch (const std::system_error& e) {
                                InetdLogger::Instance().Log(LOG_ERROR, "Failed to spawn thread for service '" + service.name + "': " + e.what());
                                closesocket(client_sock);
                                if (wait_flag) {
                                    busy_ptr->store(false);
                                }
                            }
                        } else if (service.sock_type == "dgram") {
                            InetdLogger::Instance().Log(LOG_DEBUG, "Datagram event triggered on '" + service.name + "' (UDP)");

                            if (service.wait_flag) {
                                service.is_busy->store(true);
                            }

                            auto busy_ptr = service.is_busy;
                            bool wait_flag = service.wait_flag;
                            std::string prog = service.program;
                            std::string s_name = service.name;
                            SOCKET listen_sock = service.listen_sock;

                            try {
                                std::thread([listen_sock, service, busy_ptr, wait_flag, prog, s_name]() {
                                    if (prog == "internal") {
                                        BuiltinServicesEngine::HandleUDP(listen_sock, s_name);
                                    } else {
                                        ProcessLauncher::Launch(listen_sock, service);
                                    }
                                    if (wait_flag) {
                                        busy_ptr->store(false);
                                    }
                                }).detach();
                            } catch (const std::system_error& e) {
                                InetdLogger::Instance().Log(LOG_ERROR, "Failed to spawn thread for service '" + service.name + "': " + e.what());
                                if (wait_flag) {
                                    busy_ptr->store(false);
                                }
                            }
                        }
                    }
                }
            }
        }

        InetdLogger::Instance().Log(LOG_INFO, "Shutting down listening sockets...");
        for (auto& service : services) {
            if (service.listen_sock != INVALID_SOCKET) {
                closesocket(service.listen_sock);
            }
        }

        InetdLogger::Instance().Log(LOG_INFO, "inetd stopped cleanly.");
        return 0;
    }
};
