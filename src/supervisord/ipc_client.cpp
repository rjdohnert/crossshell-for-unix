#include "ipc_client.hpp"
#include "ipc_endpoint.hpp"
#include "ipc_framing.hpp"
#include "supervisor_defaults.hpp"
#include "supervisor.hpp"

int SendIpcClientCommand(const std::string& cmd) {
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    constexpr int kMaxAttempts = SupervisorDefaults::kIpcConnectRetryCount;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        hPipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) break;

        DWORD err = GetLastError();
        if (err != ERROR_PIPE_BUSY && err != ERROR_FILE_NOT_FOUND) {
            std::cout << "Error: Could not connect to supervisord daemon. Win32 Error: " << err << "\n";
            return 10;
        }

        if (!WaitNamedPipeW(PIPE_NAME, SupervisorDefaults::kIpcBusyTimeoutMs)) {
            std::cout << "Error: Supervisord daemon pipe unavailable/busy timeout.\n";
            return 11;
        }
    }

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "Error: Could not connect to supervisord daemon after retries.\n";
        return 12;
    }

    DWORD written = 0;
    if (!WriteIpcFrame(hPipe, cmd)) {
        std::cout << "Error: Failed to send command to daemon. Win32 Error: " << GetLastError() << "\n";
        CloseHandle(hPipe);
        return 13;
    }

    std::string response;
    if (!ReadIpcFrame(hPipe, response)) {
        std::cout << "Error: Failed to read framed response from daemon.\n";
        CloseHandle(hPipe);
        return 15;
    }
    CloseHandle(hPipe);

    if (response.empty()) {
        std::cout << "Error: Empty response from daemon.\n";
        return 14;
    }

    size_t nl = response.find('\n');
    std::string header = (nl == std::string::npos) ? response : response.substr(0, nl);
    std::string payload = (nl == std::string::npos) ? std::string() : response.substr(nl + 1);

    if (header.rfind("OK|", 0) == 0) {
        if (header.rfind("OK|STATUS", 0) == 0 || header.rfind("OK|DIAG", 0) == 0 || header.rfind("OK|METRICS", 0) == 0) {
            std::cout << payload;
        } else {
            std::vector<std::string> parts;
            std::stringstream hs(header);
            std::string piece;
            while (std::getline(hs, piece, '|')) parts.push_back(piece);
            if (parts.size() >= 3) std::cout << parts[2] << "\n";
            else std::cout << "OK\n";
        }
        return 0;
    }

    if (header.rfind("ERR|", 0) == 0) {
        std::vector<std::string> parts;
        std::stringstream hs(header);
        std::string piece;
        while (std::getline(hs, piece, '|')) parts.push_back(piece);
        if (parts.size() >= 3) {
            std::cout << "Error [" << parts[1] << "]: " << parts[2] << "\n";
        } else {
            std::cout << "Error: " << header << "\n";
        }
        return 20;
    }

    std::cout << response;
    return 0;
}
