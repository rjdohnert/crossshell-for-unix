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

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// 1. RAII SCOPE & SOCKET WRAPPERS
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

    bool IsValid() const { return m_socket != INVALID_SOCKET; }
    SOCKET Get() const { return m_socket; }
    operator SOCKET() const { return m_socket; }

    void Close() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

    void Reset(SOCKET sock = INVALID_SOCKET) {
        Close();
        m_socket = sock;
    }

private:
    SOCKET m_socket;
};

// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class WhoisOptions {
public:
    std::string server;
    std::string port = "43";
    bool autoFollow = true;
    std::string target;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    target = argv[i] ? argv[i] : "";
                }
                break;
            } else if (arg == "-h" || arg == "--help") {
                showHelp = true;
                return true;
            } else if (arg == "--version" || arg == "-V") {
                showVersion = true;
                return true;
            } else if ((arg == "-H" || arg == "--host") && i + 1 < argc) {
                server = argv[++i];
            } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
                port = argv[++i];
            } else if (arg == "-a") {
                server = "whois.arin.net";
            } else if (arg == "-A") {
                server = "whois.apnic.net";
            } else if (arg == "-c") {
                server = "whois.lacnic.net";
            } else if (arg == "-r") {
                server = "whois.ripe.net";
            } else if (arg == "-d") {
                server = "whois.radb.net";
            } else if (arg == "-I") {
                server = "whois.iana.org";
            } else if (arg == "-f" || arg == "--no-follow") {
                autoFollow = false;
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "whois: unknown option -- " << arg << "\n";
                return false;
            } else {
                target = arg;
            }
        }

        if (target.empty() && !showHelp && !showVersion) {
            return false;
        }

        if (server.empty()) {
            server = "whois.iana.org";
        }

        return true;
    }

    void PrintUsage(const char* progName) const {
        std::cout << R"(whois(1)                CrossShell for UNIX Reference Manual                 whois(1)

    NAME
        whois - client for the WHOIS directory service

    SYNOPSIS
        whois [OPTIONS] OBJECT

    DESCRIPTION
        whois searches for an object in a RFC 3912 WHOIS database server.

    OPTIONS
        -H, --host HOST
            Connect to server HOST.

        -p, --port PORT
            Connect to port PORT (default 43).

        -a
            Query ARIN database.

        -A
            Query APNIC database.

        -r
            Query RIPE database.

        --json, --csv, --table
            Output WHOIS response records as JSON, CSV, or table.

        --pipe COMMAND
            Send query output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        whois example.com
            Query WHOIS database for example.com.

    CrossShell for UNIX                                                  whois(1)
)";
    }

    void PrintVersion() const {
        std::cout << "whois 1.0.0\n";
    }
};

// ============================================================================
// 3. WHOIS PROTOCOL CLIENT & PARSER
// ============================================================================

class WhoisResponseParser {
public:
    static std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static std::string ExtractReferralServer(const std::string& response) {
        std::istringstream stream(response);
        const std::vector<std::string> keywords = {
            "Registrar WHOIS Server:",
            "Whois Server:",
            "WHOIS Server:",
            "whois:",
            "refer:"
        };

        for (std::string line; std::getline(stream, line); ) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            for (const auto& kw : keywords) {
                size_t pos = line.find(kw);
                if (pos != std::string::npos) {
                    std::string server = line.substr(pos + kw.length());
                    server = Trim(server);

                    if (server.rfind("whois://", 0) == 0) {
                        server = server.substr(8);
                    }

                    std::transform(server.begin(), server.end(), server.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });

                    if (server.find('.') != std::string::npos && server.find(' ') == std::string::npos) {
                        return server;
                    }
                }
            }
        }
        return "";
    }
};

class WhoisClient {
public:
    static std::string QueryServer(const std::string& server, const std::string& port, const std::string& query) {
        addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* res = nullptr;
        if (getaddrinfo(server.c_str(), port.c_str(), &hints, &res) != 0) {
            return "Error: Cannot resolve WHOIS server address: " + server;
        }

        ScopedSocket sock;
        for (addrinfo* ptr = res; ptr != nullptr; ptr = ptr->ai_next) {
            SOCKET s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (s == INVALID_SOCKET) continue;

            DWORD timeout = 5000;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

            if (connect(s, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == SOCKET_ERROR) {
                closesocket(s);
                continue;
            }

            sock.Reset(s);
            break;
        }

        freeaddrinfo(res);

        if (!sock.IsValid()) {
            return "Error: Failed to connect to " + server + ":" + port;
        }

        std::string request = query + "\r\n";
        if (send(sock.Get(), request.c_str(), static_cast<int>(request.length()), 0) == SOCKET_ERROR) {
            return "Error: Failed to send query.";
        }

        std::string response;
        char buffer[4096];
        int bytesReceived = 0;
        while ((bytesReceived = recv(sock.Get(), buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesReceived] = '\0';
            response.append(buffer, static_cast<size_t>(bytesReceived));
        }

        return response;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class WhoisApplication {
public:
    int Run(int argc, char* argv[]) {
        WhoisOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : "whois");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : "whois");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::cerr << "whois: Winsock initialization failed\n";
            return 1;
        }

        std::string currentServer = opts.server;
        std::vector<std::string> visitedServers;
        int maxReferrals = 4;

        while (!currentServer.empty() && maxReferrals-- > 0) {
            if (std::find(visitedServers.begin(), visitedServers.end(), currentServer) != visitedServers.end()) {
                break;
            }
            visitedServers.push_back(currentServer);

            std::cout << "[Querying " << currentServer << ":" << opts.port << " for '" << opts.target << "']\n\n";

            std::string response = WhoisClient::QueryServer(currentServer, opts.port, opts.target);

            if (response.rfind("Error:", 0) == 0) {
                std::cerr << response << "\n";
                return 1;
            }

            std::cout << response << "\n";

            if (!opts.autoFollow) {
                break;
            }

            std::string nextServer = WhoisResponseParser::ExtractReferralServer(response);
            if (!nextServer.empty() && nextServer != currentServer) {
                std::cout << "\n--------------------------------------------------\n";
                std::cout << "Following referral to " << nextServer << "...\n";
                std::cout << "--------------------------------------------------\n\n";
                currentServer = nextServer;
            } else {
                break;
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    WhoisApplication app;
    return app.Run(argc, argv);
}
