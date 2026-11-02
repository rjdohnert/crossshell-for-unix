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

            // Set 3 second connect/recv timeout so offline targets fail fast without hanging
            DWORD timeoutMs = 3000;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

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
        char buf[4096];
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
    HyperVPipeTransport(std::string name) : pipeName(std::move(name)) {}

    ~HyperVPipeTransport() override {
        Close();
    }

    bool Open() override {
        Close();
        std::string fullPath = "\\\\.\\pipe\\" + pipeName;
        hPipe = CreateFileA(fullPath.c_str(), GENERIC_READ | GENERIC_WRITE,
                            0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe == INVALID_HANDLE_VALUE) {
            return false;
        }
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
        char buf[4096];
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

    static uint64_t ParseHex64LittleEndian(const std::string& hex) {
        if (hex.size() < 16) return 0;
        uint64_t val = 0;
        for (int i = 7; i >= 0; --i) {
            std::string byteStr = hex.substr(i * 2, 2);
            try {
                uint8_t b = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
                val = (val << 8) | b;
            } catch (...) {
                return 0;
            }
        }
        return val;
    }

    // Parse GDB x86_64 'g' register response packet
    static bool ParseGRegisters(const std::string& reply, CpuContext64& ctx) {
        size_t start = reply.find('$');
        size_t end = reply.find('#');
        if (start == std::string::npos || end == std::string::npos || end <= start + 1) {
            return false;
        }
        std::string payload = reply.substr(start + 1, end - start - 1);
        // Minimum payload size: 17 registers * 16 hex characters = 272 characters (covers rax through rip).
        if (payload.empty() || payload[0] == 'E' || payload.size() < 17 * 16) {
            return false;
        }

        // GDB x86_64 register layout:
        // 0: rax, 1: rbx, 2: rcx, 3: rdx, 4: rsi, 5: rdi, 6: rbp, 7: rsp
        // 8: r8, 9: r9, 10: r10, 11: r11, 12: r12, 13: r13, 14: r14, 15: r15
        // 16: rip, 17: rflags
        ctx.rax = ParseHex64LittleEndian(payload.substr(0 * 16, 16));
        ctx.rbx = ParseHex64LittleEndian(payload.substr(1 * 16, 16));
        ctx.rcx = ParseHex64LittleEndian(payload.substr(2 * 16, 16));
        ctx.rdx = ParseHex64LittleEndian(payload.substr(3 * 16, 16));
        ctx.rsi = ParseHex64LittleEndian(payload.substr(4 * 16, 16));
        ctx.rdi = ParseHex64LittleEndian(payload.substr(5 * 16, 16));
        ctx.rbp = ParseHex64LittleEndian(payload.substr(6 * 16, 16));
        ctx.rsp = ParseHex64LittleEndian(payload.substr(7 * 16, 16));
        ctx.r8  = ParseHex64LittleEndian(payload.substr(8 * 16, 16));
        ctx.r9  = ParseHex64LittleEndian(payload.substr(9 * 16, 16));
        ctx.r10 = ParseHex64LittleEndian(payload.substr(10 * 16, 16));
        ctx.r11 = ParseHex64LittleEndian(payload.substr(11 * 16, 16));
        ctx.r12 = ParseHex64LittleEndian(payload.substr(12 * 16, 16));
        ctx.r13 = ParseHex64LittleEndian(payload.substr(13 * 16, 16));
        ctx.r14 = ParseHex64LittleEndian(payload.substr(14 * 16, 16));
        ctx.r15 = ParseHex64LittleEndian(payload.substr(15 * 16, 16));
        ctx.rip = ParseHex64LittleEndian(payload.substr(16 * 16, 16));

        // Register 17 (rflags) requires 18 * 16 = 288 hex chars. If a minimalist stub
        // omits rflags (providing only 272 chars for rax-rip), rflags defaults to 0
        // while the core execution context is still marked valid=true.
        if (payload.size() >= 18 * 16) {
            ctx.rflags = ParseHex64LittleEndian(payload.substr(17 * 16, 16));
        } else {
            ctx.rflags = 0;
        }

        ctx.valid = true;
        return true;
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
