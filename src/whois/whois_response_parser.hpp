#pragma once

#include "whois.hpp"

class WhoisResponseParser {
public:
    static std::string Trim(const std::string& str);

    static std::string ExtractReferralServer(const std::string& response);
};
