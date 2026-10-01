/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fcntl.h>
#include <chrono>
#include <csignal>
#include <ctime>
#include <fstream>
#include <iostream>
#include <io.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. DATA MODELS & RAII SCOPES
// ============================================================================

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            m_initialized = true;
        }
    }

    ~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class ScopedSocket {
public:
    explicit ScopedSocket(SOCKET sock = INVALID_SOCKET) : m_socket(sock) {}

    ~ScopedSocket() {
        Close();
    }

    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    ScopedSocket(ScopedSocket&& other) noexcept : m_socket(other.m_socket) {
        other.m_socket = INVALID_SOCKET;
    }

    ScopedSocket& operator=(ScopedSocket&& other) noexcept {
        if (this != &other) {
            Close();
            m_socket = other.m_socket;
            other.m_socket = INVALID_SOCKET;
        }
        return *this;
    }

    SOCKET Get() const { return m_socket; }
    bool IsValid() const { return m_socket != INVALID_SOCKET; }

    void Close() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    void Reset(SOCKET s = INVALID_SOCKET) {
        Close();
        m_socket = s;
    }

private:
    SOCKET m_socket;
};

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };

class InetdLogger {
public:
    static InetdLogger& Instance() {
        static InetdLogger instance;
        return instance;
    }

    void SetDebug(bool debug) { m_debug = debug; }
    void SetVerbose(bool verbose) { m_verbose = verbose; }
    bool IsDebug() const { return m_debug; }
    bool IsVerbose() const { return m_verbose; }

    void Log(LogLevel level, const std::string& msg) {
        if (level == LOG_DEBUG && !m_debug) return;
        if (level == LOG_INFO && !m_verbose && !m_debug) return;

        std::lock_guard<std::mutex> lock(m_mutex);
        SYSTEMTIME st;
        GetLocalTime(&st);

        const char* level_str = "INFO";
        if (level == LOG_DEBUG) level_str = "DEBUG";
        if (level == LOG_WARN)  level_str = "WARN";
        if (level == LOG_ERROR) level_str = "ERROR";

        char time_buf[32];
        snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        std::cout << "[" << time_buf << "] [" << level_str << "] " << msg << std::endl;
    }

private:
    std::mutex m_mutex;
    bool m_debug = false;
    bool m_verbose = false;
};

class AdminPrivilegeChecker {
public:
    static bool IsRunningAsAdmin() {
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
};

struct ServiceConfig {
    std::string name;
    std::string sock_type;  // stream / dgram
    std::string protocol;   // tcp / udp
    std::string port_str;   // service name or port number
    bool wait_flag = false; // false = nowait, true = wait
    std::string user;       // Solaris compatibility field
    std::string program;    // Path or "internal"
    std::string args;
    SOCKET listen_sock = INVALID_SOCKET;

    // Shared atomic lock to prevent select() from handling 'wait' sockets while active
    std::shared_ptr<std::atomic<bool>> is_busy = std::make_shared<std::atomic<bool>>(false);
};

// ============================================================================
// 2. CONFIGURATION PARSER & STRING UTILS
// ============================================================================

class InetdConfigParser {
public:
    static std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static std::vector<std::string> SplitConfigLine(const std::string& str) {
        std::vector<std::string> tokens;
        std::string current;
        bool in_quotes = false;
        for (size_t i = 0; i < str.length(); ++i) {
            char c = str[i];
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        return tokens;
    }

    static bool Parse(const std::string& filepath, std::vector<ServiceConfig>& services) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            InetdLogger::Instance().Log(LOG_ERROR, "Unable to open configuration file: " + filepath);
            return false;
        }

        std::string line;
        size_t line_num = 0;
        while (std::getline(file, line)) {
            line_num++;
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            std::string trimmed = Trim(line);
            if (trimmed.empty()) continue;

            auto tokens = SplitConfigLine(trimmed);
            if (tokens.size() < 6) {
                InetdLogger::Instance().Log(LOG_WARN, "Malformed line " + std::to_string(line_num) + " in " + filepath);
                continue;
            }

            ServiceConfig cfg;
            cfg.name      = tokens[0];
            cfg.sock_type = tokens[1];
            cfg.protocol  = tokens[2];
            cfg.wait_flag = (tokens[3] == "wait");
            cfg.user      = tokens[4];
            cfg.program   = tokens[5];

            for (size_t i = 6; i < tokens.size(); ++i) {
                if (i > 6) cfg.args += " ";
                cfg.args += tokens[i];
            }

            if (cfg.sock_type == "dgram" && !cfg.wait_flag) {
                InetdLogger::Instance().Log(LOG_WARN, "UDP service '" + cfg.name + "' is configured as nowait, forcing to wait mode to avoid thread storms.");
                cfg.wait_flag = true;
            }

            cfg.port_str = cfg.name;
            services.push_back(cfg);

            InetdLogger::Instance().Log(LOG_INFO, "Registered service '" + cfg.name + "' (" + cfg.protocol + ") -> " + cfg.program);
        }

        return !services.empty();
    }
};

// ============================================================================
// 3. BUILT-IN SERVICES & PROCESS LAUNCHER
// ============================================================================

class BuiltinServicesEngine {
public:
    static void HandleTCP(SOCKET client_sock, const std::string& service_name) {
        if (service_name == "echo") {
            char buffer[1024];
            int bytes_read;
            while ((bytes_read = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
                send(client_sock, buffer, bytes_read, 0);
            }
        } else if (service_name == "daytime") {
            std::time_t now = std::time(nullptr);
            char time_str[64];
            ctime_s(time_str, sizeof(time_str), &now);
            send(client_sock, time_str, static_cast<int>(strlen(time_str)), 0);
        } else if (service_name == "discard") {
            char buffer[1024];
            while (recv(client_sock, buffer, sizeof(buffer), 0) > 0) {
                // Discard data
            }
        }
        closesocket(client_sock);
    }

    static void HandleUDP(SOCKET sock, const std::string& service_name) {
        char buffer[2048];
        sockaddr_storage client_addr;
        int addr_len = sizeof(client_addr);
        int bytes_read = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (bytes_read <= 0) return;

        if (service_name == "echo") {
            sendto(sock, buffer, bytes_read, 0, reinterpret_cast<sockaddr*>(&client_addr), addr_len);
        } else if (service_name == "daytime") {
            std::time_t now = std::time(nullptr);
            char time_str[64];
            ctime_s(time_str, sizeof(time_str), &now);
            sendto(sock, time_str, static_cast<int>(strlen(time_str)), 0, reinterpret_cast<sockaddr*>(&client_addr), addr_len);
        } else if (service_name == "discard") {
            // Discard datagram
        }
    }
};

class ProcessLauncher {
public:
    static void Launch(SOCKET sock, const ServiceConfig& config) {
        if (!SetHandleInformation(reinterpret_cast<HANDLE>(sock), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
            InetdLogger::Instance().Log(LOG_ERROR, "Failed to set handle inheritance on socket. Win32 Error: " + std::to_string(GetLastError()));
            if (config.sock_type == "stream") closesocket(sock);
            return;
        }

        STARTUPINFOEXA siEx = {};
        PROCESS_INFORMATION pi = {};
        siEx.StartupInfo.cb = sizeof(siEx);
        siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        siEx.StartupInfo.hStdInput  = reinterpret_cast<HANDLE>(sock);
        siEx.StartupInfo.hStdOutput = reinterpret_cast<HANDLE>(sock);
        siEx.StartupInfo.hStdError  = reinterpret_cast<HANDLE>(sock);

        SIZE_T size = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &size);
        std::vector<BYTE> attributeList(size);
        siEx.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeList.data());

        if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size)) {
            InetdLogger::Instance().Log(LOG_ERROR, "InitializeProcThreadAttributeList failed. Win32 Error: " + std::to_string(GetLastError()));
            if (config.sock_type == "stream") closesocket(sock);
            return;
        }

        HANDLE handlesToInherit[1] = { reinterpret_cast<HANDLE>(sock) };
        if (!UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       handlesToInherit, sizeof(handlesToInherit), NULL, NULL)) {
            InetdLogger::Instance().Log(LOG_ERROR, "UpdateProcThreadAttribute failed. Win32 Error: " + std::to_string(GetLastError()));
            DeleteProcThreadAttributeList(siEx.lpAttributeList);
            if (config.sock_type == "stream") closesocket(sock);
            return;
        }

        std::string cmdline;
        if (config.program.find(' ') != std::string::npos && config.program.front() != '"') {
            cmdline = "\"" + config.program + "\"";
        } else {
            cmdline = config.program;
        }
        if (!config.args.empty()) {
            cmdline += " " + config.args;
        }

        std::vector<char> cmd_buffer(cmdline.begin(), cmdline.end());
        cmd_buffer.push_back('\0');

        BOOL success = CreateProcessA(
            NULL,
            cmd_buffer.data(),
            NULL,
            NULL,
            TRUE,
            EXTENDED_STARTUPINFO_PRESENT,
            NULL,
            NULL,
            &siEx.StartupInfo,
            &pi
        );

        DeleteProcThreadAttributeList(siEx.lpAttributeList);

        if (!success) {
            InetdLogger::Instance().Log(LOG_ERROR, "Failed to launch program '" + config.program + "'. Win32 Error: " + std::to_string(GetLastError()));
            if (config.sock_type == "stream") closesocket(sock);
            return;
        }

        InetdLogger::Instance().Log(LOG_DEBUG, "Spawned child process PID " + std::to_string(pi.dwProcessId) + " for service '" + config.name + "'");

        if (config.wait_flag) {
            WaitForSingleObject(pi.hProcess, INFINITE);
        }

        if (config.sock_type == "stream") {
            closesocket(sock);
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
};

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

int main(int argc, char* argv[]) {
    InetdApplication app;
    return app.Run(argc, argv);
}
