#ifndef NODENAME_ENGINE_HPP
#define NODENAME_ENGINE_HPP

#include "nodename.hpp"

class SystemNodeManager {
public:
    static std::wstring QueryName(NameQueryMode mode);
    static bool SetNodeName(const std::wstring& newHostname, DWORD& outError);
};

#endif // NODENAME_ENGINE_HPP
