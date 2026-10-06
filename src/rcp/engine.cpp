#include "engine.hpp"

// RcpSocket
RcpSocket::RcpSocket() : m_sock(INVALID_SOCKET) {}
RcpSocket::~RcpSocket() { Close(); }

bool RcpSocket::Connect(const std::string& host, const std::string& port) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) {
        std::cerr << "[ERROR] Failed to resolve host: " << host << "\n";
        return false;
    }

    for (addrinfo* ptr = res; ptr != nullptr; ptr = ptr->ai_next) {
        m_sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (m_sock == INVALID_SOCKET) continue;

        DWORD timeout = SOCKET_TIMEOUT_MS;
        setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(m_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        if (ptr->ai_family == AF_INET) {
            sockaddr_in local_sin{};
            local_sin.sin_family = AF_INET;
            local_sin.sin_addr.s_addr = INADDR_ANY;

            bool bound_priv = false;
            for (unsigned short priv_port = 1023; priv_port >= 512; --priv_port) {
                local_sin.sin_port = htons(priv_port);
                if (bind(m_sock, (sockaddr*)&local_sin, sizeof(local_sin)) == 0) {
                    bound_priv = true;
                    break;
                }
            }
            if (!bound_priv && !StringUtils::IsRunningAsAdmin()) {
                std::cerr << "[NOTICE] Running without elevated privileges. Standard ephemeral port bound.\n";
            }
        } else if (ptr->ai_family == AF_INET6) {
            sockaddr_in6 local_sin6{};
            local_sin6.sin6_family = AF_INET6;
            local_sin6.sin6_addr = in6addr_any;

            bool bound_priv = false;
            for (unsigned short priv_port = 1023; priv_port >= 512; --priv_port) {
                local_sin6.sin6_port = htons(priv_port);
                if (bind(m_sock, (sockaddr*)&local_sin6, sizeof(local_sin6)) == 0) {
                    bound_priv = true;
                    break;
                }
            }
            if (!bound_priv && !StringUtils::IsRunningAsAdmin()) {
                std::cerr << "[NOTICE] Running without elevated privileges. Standard ephemeral port bound.\n";
            }
        }

        if (connect(m_sock, ptr->ai_addr, (int)ptr->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return true;
        }
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    freeaddrinfo(res);
    std::cerr << "[ERROR] Connection failed to " << host << ":" << port << "\n";
    return false;
}

void RcpSocket::Close() {
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
}

bool RcpSocket::SendAll(const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    while (len > 0) {
        int sent = send(m_sock, ptr, static_cast<int>(len), 0);
        if (sent <= 0) return false;
        ptr += sent;
        len -= sent;
    }
    return true;
}

bool RcpSocket::SendString(const std::string& str) {
    return SendAll(str.c_str(), str.length());
}

bool RcpSocket::SendByte(uint8_t byte) {
    return SendAll(&byte, 1);
}

bool RcpSocket::ReadByte(uint8_t& byte) {
    int r = recv(m_sock, reinterpret_cast<char*>(&byte), 1, 0);
    return r == 1;
}

bool RcpSocket::ReadLine(std::string& line) {
    line.clear();
    char c;
    while (recv(m_sock, &c, 1, 0) == 1) {
        if (c == '\n') return true;
        line += c;
    }
    return !line.empty();
}

bool RcpSocket::CheckAck() {
    uint8_t status;
    if (!ReadByte(status)) {
        std::cerr << "[ERROR] Timeout or connection dropped while waiting for ACK.\n";
        return false;
    }
    if (status == 0) return true;

    std::string err_msg;
    ReadLine(err_msg);
    std::cerr << "[REMOTE ERROR] " << err_msg << "\n";
    return false;
}

SOCKET RcpSocket::GetRaw() const { return m_sock; }

// RcpTransferEngine
bool RcpTransferEngine::SendLocalFile(RcpSocket& sock, const fs::path& filepath, const RcpOptions& opt) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Cannot open local file: " << StringUtils::WStringToString(filepath.wstring()) << "\n";
        return false;
    }

    uint64_t filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (opt.preserve) {
        int64_t mtime = 0, atime = 0;
        if (FileTimestampHelper::GetFileTimestampsWin32(filepath, mtime, atime)) {
            std::ostringstream t_stream;
            t_stream << "T" << mtime << " 0 " << atime << " 0\n";
            if (!sock.SendString(t_stream.str()) || !sock.CheckAck()) {
                return false;
            }
        }
    }

    std::string filename = StringUtils::WStringToString(filepath.filename().wstring());
    std::ostringstream c_stream;
    c_stream << "C0644 " << filesize << " " << filename << "\n";

    std::cout << "--> Sending: " << filename << " (" << filesize << " bytes)... ";

    if (!sock.SendString(c_stream.str()) || !sock.CheckAck()) {
        std::cout << "[FAILED]\n";
        return false;
    }

    std::vector<char> buffer(BUFFER_SIZE);
    uint64_t remaining = filesize;
    while (remaining > 0) {
        size_t chunk = static_cast<size_t>((static_cast<uint64_t>(buffer.size()) < remaining) ? static_cast<uint64_t>(buffer.size()) : remaining);
        file.read(buffer.data(), chunk);
        if (!sock.SendAll(buffer.data(), chunk)) {
            std::cout << "[FAILED]\n";
            return false;
        }
        remaining -= chunk;
    }

    sock.SendByte(0);
    if (!sock.CheckAck()) {
        std::cout << "[FAILED]\n";
        return false;
    }

    std::cout << "[OK]\n";
    return true;
}

bool RcpTransferEngine::SendLocalDirectory(RcpSocket& sock, const fs::path& dirpath, const RcpOptions& opt) {
    std::string dirname = StringUtils::WStringToString(dirpath.filename().wstring());
    std::ostringstream d_stream;
    d_stream << "D0755 0 " << dirname << "\n";

    std::cout << "==> Entering Directory: " << dirname << "\n";
    if (!sock.SendString(d_stream.str()) || !sock.CheckAck()) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(dirpath)) {
        std::error_code ec;
        const auto status = entry.symlink_status(ec);
        if (!ec) {
            if (fs::is_directory(status)) {
                if (opt.recursive) {
                    if (!SendLocalDirectory(sock, entry.path(), opt)) return false;
                }
            } else if (fs::is_regular_file(status)) {
                if (!SendLocalFile(sock, entry.path(), opt)) return false;
            }
        }
    }

    std::cout << "<== Leaving Directory: " << dirname << "\n";
    return sock.SendString("E\n") && sock.CheckAck();
}

bool RcpTransferEngine::ReceiveStream(RcpSocket& sock, const fs::path& local_target, const RcpOptions& opt) {
    sock.SendByte(0);

    uint8_t cmd_type;
    int64_t mtime = 0, atime = 0;

    while (sock.ReadByte(cmd_type)) {
        if (cmd_type == 0) continue;

        if (cmd_type == 1 || cmd_type == 2) {
            std::string err;
            sock.ReadLine(err);
            std::cerr << "[REMOTE ERROR] " << err << "\n";
            return false;
        }

        std::string line;
        if (!sock.ReadLine(line)) break;

        if (cmd_type == 'T') {
            std::istringstream iss(line);
            iss >> mtime >> atime;
            sock.SendByte(0);
        } else if (cmd_type == 'C' || cmd_type == 'D') {
            std::istringstream iss(line);
            std::string mode_str;
            uint64_t size = 0;
            iss >> mode_str >> size;

            std::string raw_name;
            std::getline(iss >> std::ws, raw_name);

            std::string safe_name = StringUtils::SanitizeIncomingFilename(raw_name);
            fs::path final_path = fs::is_directory(local_target) ? (local_target / StringUtils::Utf8ToPath(safe_name)) : local_target;

            if (cmd_type == 'D') {
                std::cout << "==> Creating Directory: " << StringUtils::WStringToString(final_path.wstring()) << "\n";
                fs::create_directories(final_path);
                sock.SendByte(0);
                if (!ReceiveStream(sock, final_path, opt)) return false;
            } else {
                std::cout << "<-- Receiving: " << safe_name << " (" << size << " bytes)... ";

                fs::path temp_path = StringUtils::Utf8ToPath(final_path.string() + ".tmp");
                TempFileTracker::Instance().Register(temp_path);

                std::ofstream ofs(temp_path, std::ios::binary);
                if (!ofs.is_open()) {
                    std::cout << "[FAILED]\n";
                    std::cerr << "[ERROR] Cannot create local file: " << StringUtils::WStringToString(temp_path.wstring()) << "\n";
                    TempFileTracker::Instance().Unregister(temp_path);
                    return false;
                }

                sock.SendByte(0);

                std::vector<char> buffer(BUFFER_SIZE);
                uint64_t remaining = size;
                while (remaining > 0) {
                    int chunk = static_cast<int>((static_cast<uint64_t>(buffer.size()) < remaining) ? static_cast<uint64_t>(buffer.size()) : remaining);
                    int bytes_read = recv(sock.GetRaw(), buffer.data(), chunk, 0);
                    if (bytes_read <= 0) {
                        std::cout << "[FAILED]\n";
                        ofs.close();
                        TempFileTracker::Instance().Unregister(temp_path);
                        std::error_code ec;
                        fs::remove(temp_path, ec);
                        return false;
                    }
                    ofs.write(buffer.data(), bytes_read);
                    if (!ofs) {
                        std::cout << "[FAILED - WRITE ERROR]\n";
                        ofs.close();
                        TempFileTracker::Instance().Unregister(temp_path);
                        std::error_code ec;
                        fs::remove(temp_path, ec);
                        return false;
                    }
                    remaining -= bytes_read;
                }

                ofs.close();

                if (!sock.CheckAck()) {
                    std::cout << "[FAILED]\n";
                    TempFileTracker::Instance().Unregister(temp_path);
                    std::error_code ec;
                    fs::remove(temp_path, ec);
                    return false;
                }

                std::error_code ec;
                fs::remove(final_path, ec);
                fs::rename(temp_path, final_path, ec);
                TempFileTracker::Instance().Unregister(temp_path);

                if (ec) {
                    std::cout << "[FAILED - RENAME ERROR]\n";
                    return false;
                }

                if (opt.preserve && mtime > 0) {
                    FileTimestampHelper::SetFileTimestampsWin32(final_path, mtime, atime > 0 ? atime : mtime);
                }

                std::cout << "[OK]\n";
                sock.SendByte(0);
            }
        } else if (cmd_type == 'E') {
            sock.SendByte(0);
            return true;
        }
    }
    return true;
}
