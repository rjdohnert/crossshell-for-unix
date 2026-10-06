#pragma once

#include "resolve.hpp"

class ResolveEngine {
private:
    std::string m_dnsServer;
    std::string m_queryTypeStr;
    WORD m_queryType;
    bool m_debug;
    std::map<std::string, WORD> m_queryTypes;

public:
    ResolveEngine();

    void SetServer(const std::string& serverInput);

    std::string GetServer() const;

    bool SetQueryType(const std::string& type);

    std::string GetQueryType() const;

    void SetDebug(bool debug);
    bool GetDebug() const;

    bool ExecuteQuery(const std::string& targetInput);
};
