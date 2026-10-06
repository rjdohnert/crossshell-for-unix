#include "engine.hpp"

std::atomic<SOCKET> g_active_socket{ INVALID_SOCKET };

BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    (void)dwCtrlType;
    SOCKET s = g_active_socket.exchange(INVALID_SOCKET);
    if (s != INVALID_SOCKET) {
        shutdown(s, SD_BOTH);
        closesocket(s);
    }
    WSACleanup();
    return FALSE;
}

WinsockScope::WinsockScope() : m_initialized(false) {
    WSADATA wsaData = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_initialized = true;
    }
}

WinsockScope::~WinsockScope() {
    if (m_initialized) {
        WSACleanup();
    }
}

bool WinsockScope::IsInitialized() const {
    return m_initialized;
}

ScopedSocket::ScopedSocket(SOCKET sock) : m_socket(sock) {}

ScopedSocket::~ScopedSocket() {
    Close();
}

ScopedSocket::ScopedSocket(ScopedSocket&& other) noexcept : m_socket(other.m_socket) {
    other.m_socket = INVALID_SOCKET;
}

ScopedSocket& ScopedSocket::operator=(ScopedSocket&& other) noexcept {
    if (this != &other) {
        Close();
        m_socket = other.m_socket;
        other.m_socket = INVALID_SOCKET;
    }
    return *this;
}

SOCKET ScopedSocket::Get() const {
    return m_socket;
}

bool ScopedSocket::IsValid() const {
    return m_socket != INVALID_SOCKET;
}

void ScopedSocket::Close() {
    if (m_socket != INVALID_SOCKET) {
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

void ScopedSocket::Reset(SOCKET s) {
    Close();
    m_socket = s;
}

bool NetcatPipeline::ConnectWithTimeout(SOCKET sock, const sockaddr* addr, int addrlen, int timeout_sec) {
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

bool NetcatPipeline::ExecuteCommand(SOCKET sock, const std::string& cmd) {
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

void NetcatPipeline::RelayIo(SOCKET sock, const NcOptions& opt) {
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
