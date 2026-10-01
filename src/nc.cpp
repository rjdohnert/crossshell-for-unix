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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
Single-File Index
-----------------
1. Platform and standard library includes
2. Global options and control-state declarations
3. Help output and argument parsing utilities
4. Socket setup, connect/listen, and timeout helpers
5. Data forwarding and optional command execution pipeline
6. Program entry point and mode dispatch
*/

//netcat v3.4.7 


#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mstcpip.h>
#include <io.h>
#include <fcntl.h>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

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
            shutdown(m_socket, SD_BOTH);
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

struct NcOptions {
    bool ipv4_only = false;
    bool ipv6_only = false;
    bool detach_stdin = false;
    bool listen = false;
    bool keep_open = false;
    bool no_dns = false;
    bool udp = false;
    int verbose = 0;
    bool zero_io = false;
    std::string exec_cmd;
    int interval = 0;
    int timeout = 0;
    int local_port = 0;
    std::string source_ip;
    std::string destination;
    std::vector<int> ports;
    bool showHelp = false;
    bool showVersion = false;

    void ParsePorts(const std::string& arg) {
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

    bool Parse(int argc, char* argv[]) {
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

    void PrintHelp() const {
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

    void PrintVersion() const {
        std::cout << "nc 3.4.7\n";
    }
};

// ============================================================================
// 2. COMMAND EXECUTION & RELAY ENGINE
// ============================================================================

class NetcatPipeline {
public:
    static bool ConnectWithTimeout(SOCKET sock, const sockaddr* addr, int addrlen, int timeout_sec) {
        if (timeout_sec <= 0) return connect(sock, addr, addrlen) == 0;

        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);

        int res = connect(sock, addr, addrlen);
        if (res == 0) {
            mode = 0;
            ioctlsocket(sock, FIONBIO, &mode);
            return true;
        }

        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            mode = 0;
            ioctlsocket(sock, FIONBIO, &mode);
            return false;
        }

        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);

        timeval tv = {};
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;

        if (select(0, NULL, &writefds, NULL, &tv) > 0 && FD_ISSET(sock, &writefds)) {
            int so_error = 0, len = sizeof(so_error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
            mode = 0;
            ioctlsocket(sock, FIONBIO, &mode);
            return so_error == 0;
        }

        mode = 0;
        ioctlsocket(sock, FIONBIO, &mode);
        return false;
    }

    static bool ExecuteCommand(SOCKET sock, const std::string& cmd) {
        HANDLE hChildStdinRead = NULL, hChildStdinWrite = NULL;
        HANDLE hChildStdoutRead = NULL, hChildStdoutWrite = NULL;

        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hChildStdoutRead, &hChildStdoutWrite, &sa, 0)) return false;
        SetHandleInformation(hChildStdoutRead, HANDLE_FLAG_INHERIT, 0);

        if (!CreatePipe(&hChildStdinRead, &hChildStdinWrite, &sa, 0)) {
            CloseHandle(hChildStdoutRead);
            CloseHandle(hChildStdoutWrite);
            return false;
        }
        SetHandleInformation(hChildStdinWrite, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hChildStdinRead;
        si.hStdOutput = hChildStdoutWrite;
        si.hStdError = hChildStdoutWrite;

        std::vector<char> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back('\0');

        if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(hChildStdinRead);  CloseHandle(hChildStdinWrite);
            CloseHandle(hChildStdoutRead); CloseHandle(hChildStdoutWrite);
            return false;
        }

        CloseHandle(hChildStdinRead);
        CloseHandle(hChildStdoutWrite);

        std::atomic<bool> proc_running{ true };

        std::thread t_in([&]() {
            char buf[8192];
            DWORD bytesWritten;
            while (proc_running) {
                int n = recv(sock, buf, sizeof(buf), 0);
                if (n <= 0) break;
                if (!WriteFile(hChildStdinWrite, buf, n, &bytesWritten, NULL)) break;
            }
            CloseHandle(hChildStdinWrite);
        });

        std::thread t_out([&]() {
            char buf[8192];
            DWORD bytesRead;
            while (proc_running) {
                if (!ReadFile(hChildStdoutRead, buf, sizeof(buf), &bytesRead, NULL) || bytesRead == 0) break;
                int sent = 0;
                while (sent < (int)bytesRead) {
                    int n = send(sock, buf + sent, bytesRead - sent, 0);
                    if (n <= 0) break;
                    sent += n;
                }
            }
            CloseHandle(hChildStdoutRead);
        });

        WaitForSingleObject(pi.hProcess, INFINITE);
        proc_running = false;

        shutdown(sock, SD_BOTH);

        if (t_in.joinable()) t_in.join();
        if (t_out.joinable()) t_out.join();

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }

    static void RelayIo(SOCKET sock, const NcOptions& opt) {
        std::atomic<bool> running{ true };

        std::thread stdin_thread;
        if (!opt.detach_stdin) {
            stdin_thread = std::thread([&]() {
                char buffer[8192];
                HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
                DWORD bytesRead = 0;

                while (running) {
                    if (!ReadFile(hStdin, buffer, sizeof(buffer), &bytesRead, NULL) || bytesRead == 0) break;

                    int sent = 0;
                    while (sent < (int)bytesRead && running) {
                        int n = send(sock, buffer + sent, bytesRead - sent, 0);
                        if (n <= 0) { running = false; break; }
                        sent += n;
                    }
                    if (opt.interval > 0) Sleep(opt.interval * 1000);
                }
                shutdown(sock, SD_SEND);
            });
        }

        char buffer[8192];
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD bytesWritten = 0;

        while (running) {
            int n = recv(sock, buffer, sizeof(buffer), 0);
            if (n <= 0) { running = false; break; }
            WriteFile(hStdout, buffer, n, &bytesWritten, NULL);
        }

        if (!opt.detach_stdin && stdin_thread.joinable()) {
            HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
            CancelIoEx(hStdin, NULL);

            DWORD type = GetFileType(hStdin);
            if (type == FILE_TYPE_CHAR) {
                DWORD mode = 0;
                if (GetConsoleMode(hStdin, &mode)) {
                    INPUT_RECORD ir = {};
                    ir.EventType = KEY_EVENT;
                    ir.Event.KeyEvent.bKeyDown = TRUE;
                    ir.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
                    ir.Event.KeyEvent.uChar.UnicodeChar = L'\n';
                    DWORD written = 0;
                    WriteConsoleInputW(hStdin, &ir, 1, &written);
                }
            }

            stdin_thread.join();
        }
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

static std::atomic<SOCKET> g_active_socket{ INVALID_SOCKET };

static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    SOCKET s = g_active_socket.exchange(INVALID_SOCKET);
    if (s != INVALID_SOCKET) {
        shutdown(s, SD_BOTH);
        closesocket(s);
    }
    WSACleanup();
    return FALSE;
}

class NetcatApplication {
public:
    int Run(int argc, char* argv[]) {
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
};

int main(int argc, char* argv[]) {
    NetcatApplication app;
    return app.Run(argc, argv);
}