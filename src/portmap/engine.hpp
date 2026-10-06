#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "portmap.hpp"
#include "options.hpp"

class ProcessResolver {
public:
    static std::string GetProcessNameFromPid(DWORD pid);
};

class NetworkAddressFormatter {
public:
    static std::string GetTcpStateString(DWORD state);
    static std::string FormatIpv4(DWORD ip, DWORD port);
    static std::string FormatIpv6(const UCHAR* ip, DWORD port, DWORD scopeId = 0);
};

class PortTableCollector {
public:
    static void CollectAll(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg);

private:
    static void CollectTcp4(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg);
    static void CollectTcp6(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg);
    static void CollectUdp4(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg);
    static void CollectUdp6(std::vector<ConnectionEntry>& entries, const PortmapOptions& cfg);
};

#endif // ENGINE_HPP
