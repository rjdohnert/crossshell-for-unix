#include "string_utils.hpp"

std::string StringUtils::ToLower(const std::string& text) {
        std::string result = text;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return result;
    }

std::wstring StringUtils::ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstr[0], size_needed);
        return wstr;
    }

std::string StringUtils::ToNarrow(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &str[0], size_needed, NULL, NULL);
        return str;
    }

bool StringUtils::ParseRuleNumber(const std::string& text, int& value) {
        if (text.empty()) return false;
        char* end = nullptr;
        unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0' || parsed > static_cast<unsigned long>(std::numeric_limits<int>::max())) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    }

bool StringUtils::ParsePortSpec(const std::string& spec, std::string& port, LONG& protocol) {
        port.clear();
        protocol = NET_FW_IP_PROTOCOL_ANY;

        if (spec.empty()) return true;

        const size_t slashPos = spec.find('/');
        if (slashPos == std::string::npos) {
            port = spec;
            return true;
        }

        std::string protoText = ToLower(spec.substr(slashPos + 1));
        if (protoText == "tcp") {
            protocol = NET_FW_IP_PROTOCOL_TCP;
        } else if (protoText == "udp") {
            protocol = NET_FW_IP_PROTOCOL_UDP;
        } else {
            return false;
        }

        std::string portText = spec.substr(0, slashPos);
        if (portText.empty() || portText == "any") {
            return true;
        }

        char* end = nullptr;
        unsigned long parsed = std::strtoul(portText.c_str(), &end, 10);
        if (end == portText.c_str() || *end != '\0' || parsed == 0 || parsed > 65535) {
            return false;
        }

        port = portText;
        return true;
    }
