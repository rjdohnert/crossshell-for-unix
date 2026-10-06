#include "scoped_socket.hpp"
#include "whois_client.hpp"

std::string WhoisClient::QueryServer(const std::string& server, const std::string& port, const std::string& query) {
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
