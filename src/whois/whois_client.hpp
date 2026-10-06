#pragma once

#include "whois.hpp"

class WhoisClient {
public:
    static std::string QueryServer(const std::string& server, const std::string& port, const std::string& query);
};
