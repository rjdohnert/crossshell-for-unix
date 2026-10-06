#pragma once

#include "rcp.hpp"
#include "options.hpp"

class RcpSocket {
public:
    RcpSocket();
    ~RcpSocket();

    bool Connect(const std::string& host, const std::string& port);
    void Close();
    bool SendAll(const void* data, size_t len);
    bool SendString(const std::string& str);
    bool SendByte(uint8_t byte);
    bool ReadByte(uint8_t& byte);
    bool ReadLine(std::string& line);
    bool CheckAck();
    SOCKET GetRaw() const;

private:
    SOCKET m_sock;
};

class RcpTransferEngine {
public:
    static bool SendLocalFile(RcpSocket& sock, const fs::path& filepath, const RcpOptions& opt);
    static bool SendLocalDirectory(RcpSocket& sock, const fs::path& dirpath, const RcpOptions& opt);
    static bool ReceiveStream(RcpSocket& sock, const fs::path& local_target, const RcpOptions& opt);
};
