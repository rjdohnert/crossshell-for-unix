#ifndef NC_ENGINE_HPP
#define NC_ENGINE_HPP

#include "nc.hpp"
#include "options.hpp"

extern std::atomic<SOCKET> g_active_socket;
BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType);

class NetcatPipeline {
public:
    static bool ConnectWithTimeout(SOCKET sock, const sockaddr* addr, int addrlen, int timeout_sec);
    static bool ExecuteCommand(SOCKET sock, const std::string& cmd);
    static void RelayIo(SOCKET sock, const NcOptions& opt);
};

#endif // NC_ENGINE_HPP
