// --- Abstract Transport Layer ---
class IKgdbTransport {
public:
    virtual ~IKgdbTransport() = default;
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool Send(const std::string& data) = 0;
    virtual bool Receive(std::string& data) = 0;
    virtual bool IsConnected() const = 0;
};

// --- TCP Transport (Azure / Remote Win / BSD / Remote GDB / WSL) ---
class TcpTransport : public IKgdbTransport {
private:
    std::string host;
    int port;
    SOCKET sock = INVALID_SOCKET;
    bool connected = false;

public:
    TcpTransport(std::string h, int p) : host(std::move(h)), port(p) {}

    ~TcpTransport() override {
        Close();
    }

    bool Open() override {
        Close();

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        std::string portText = std::to_string(port);
        if (getaddrinfo(host.c_str(), portText.c_str(), &hints, &result) != 0) {
            return false;
        }

        bool connectedOnce = false;
        for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (sock == INVALID_SOCKET) {
                continue;
            }

            if (connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
                connectedOnce = true;
                break;
            }

            closesocket(sock);
            sock = INVALID_SOCKET;
        }

        freeaddrinfo(result);
        if (!connectedOnce) {
            return false;
        }

        connected = true;
        return true;
    }

    void Close() override {
        if (connected && sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            connected = false;
        }
    }

    bool Send(const std::string& data) override {
        if (!connected) return false;
        int total_sent = 0;
        int to_send = static_cast<int>(data.length());
        while (total_sent < to_send) {
            int sent = send(sock, data.c_str() + total_sent, to_send - total_sent, 0);
            if (sent == SOCKET_ERROR) return false;
            total_sent += sent;
        }
        return true;
    }

    bool Receive(std::string& data) override {
        if (!connected) return false;
        char buf[2048];
        int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) return false;
        buf[bytes] = '\0';
        data = std::string(buf);
        return true;
    }

    bool IsConnected() const override { return connected; }
};

// --- Hyper-V Named Pipe Transport ---
class HyperVPipeTransport : public IKgdbTransport {
private:
    std::string pipeName;
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    bool connected = false;

public:
    explicit HyperVPipeTransport(std::string name) : pipeName(std::move(name)) {}

    ~HyperVPipeTransport() override { Close(); }

    bool Open() override {
        Close();
        std::string fullPipePath = "\\\\.\\pipe\\" + pipeName;
        hPipe = CreateFileA(fullPipePath.c_str(), GENERIC_READ | GENERIC_WRITE, 
                            0, NULL, OPEN_EXISTING, 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE) return false;
        connected = true;
        return true;
    }

    void Close() override {
        if (connected && hPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(hPipe);
            hPipe = INVALID_HANDLE_VALUE;
            connected = false;
        }
    }

    bool Send(const std::string& data) override {
        if (!connected) return false;
        DWORD bytesWritten;
        if (WriteFile(hPipe, data.c_str(), static_cast<DWORD>(data.length()), &bytesWritten, NULL) == FALSE) {
            Close();
            return false;
        }
        return true;
    }

    bool Receive(std::string& data) override {
        if (!connected) return false;
        char buf[2048];
        DWORD bytesRead;
        if (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            data = std::string(buf);
            return true;
        }
        Close();
        return false;
    }

    bool IsConnected() const override { return connected; }
};

// --- GDB RSP Protocol Helper ---
class GdbRspProtocol {
public:
    static std::string ComputeChecksum(const std::string& data) {
        unsigned char sum = 0;
        for (char c : data) sum += static_cast<unsigned char>(c);
        std::ostringstream ss;
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(sum);
        return ss.str();
    }

    static std::string FormatPacket(const std::string& payload) {
        return "$" + payload + "#" + ComputeChecksum(payload);
    }
};

// --- Console Helper ---
void EnableVTMode() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
}

