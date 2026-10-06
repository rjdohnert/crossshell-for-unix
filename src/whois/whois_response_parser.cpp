#include "whois_response_parser.hpp"

std::string WhoisResponseParser::Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

std::string WhoisResponseParser::ExtractReferralServer(const std::string& response) {
        std::istringstream stream(response);
        const std::vector<std::string> keywords = {
            "Registrar WHOIS Server:",
            "Whois Server:",
            "WHOIS Server:",
            "whois:",
            "refer:"
        };

        for (std::string line; std::getline(stream, line); ) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            for (const auto& kw : keywords) {
                size_t pos = line.find(kw);
                if (pos != std::string::npos) {
                    std::string server = line.substr(pos + kw.length());
                    server = Trim(server);

                    if (server.rfind("whois://", 0) == 0) {
                        server = server.substr(8);
                    }

                    std::transform(server.begin(), server.end(), server.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });

                    if (server.find('.') != std::string::npos && server.find(' ') == std::string::npos) {
                        return server;
                    }
                }
            }
        }
        return "";
    }
