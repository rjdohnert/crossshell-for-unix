#ifndef NETMAP_ENGINE_HPP
#define NETMAP_ENGINE_HPP

#include "netmap.hpp"

namespace Netmap {

class InterfaceManager {
public:
    static std::vector<InterfaceInfo> GetAllInterfaces();
    static std::optional<InterfaceInfo> FindByIpOrIndex(const std::string& identifier);
    static std::optional<InterfaceInfo> ResolveBestInterfaceForIp(const std::string& targetIp);
};

class ArpEngine {
public:
    static bool DisplayTable(const std::optional<std::string>& ipFilter, 
                             const std::optional<std::string>& ifFilter,
                             bool verbose);
    static bool AddEntry(const std::string& ipAddress, const std::string& macAddress, 
                         const std::optional<std::string>& ifIdentifier);
    static bool DeleteEntry(const std::string& ipAddress, const std::optional<std::string>& ifIdentifier);
    static bool FlushEntries(const std::optional<std::string>& ifIdentifier);
    static bool ScanSubnet(const std::string& cidrOrRange, int timeoutMs = 800, int threadCount = 32);
};

} // namespace Netmap

#endif // NETMAP_ENGINE_HPP
