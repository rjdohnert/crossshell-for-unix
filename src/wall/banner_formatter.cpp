#include "banner_formatter.hpp"

std::string BannerFormatter::format(const std::string& user, const std::string& host, const std::string& timeStr) {
        std::ostringstream oss;
        oss << "\r\nBroadcast message from " << user << "@" << host << " (" << timeStr << "):\r\n\r\n";
        return oss.str();
    }
