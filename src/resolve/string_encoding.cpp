#include "string_encoding.hpp"

std::string StringEncoding::WideToUtf8(const wchar_t* wstr) {
        if (!wstr) return "";
        int len = static_cast<int>(wcslen(wstr));
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return "";
        std::string str(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, len, &str[0], size, nullptr, nullptr);
        return str;
    }

std::wstring StringEncoding::Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        if (size <= 0) return L"";
        std::wstring wstr(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
        if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
        return wstr;
    }

std::string StringEncoding::FormatInAddrArpa(const std::string& ipStr) {
        in_addr addr;
        if (inet_pton(AF_INET, ipStr.c_str(), &addr) == 1) {
            BYTE* bytes = reinterpret_cast<BYTE*>(&addr.s_addr);
            char buf[128];
            snprintf(buf, sizeof(buf), "%d.%d.%d.%d.in-addr.arpa",
                     bytes[3], bytes[2], bytes[1], bytes[0]);
            return std::string(buf);
        }
        return ipStr;
    }

std::string StringEncoding::FormatIp6Arpa(const std::string& ipStr) {
        in6_addr addr;
        if (inet_pton(AF_INET6, ipStr.c_str(), &addr) == 1) {
            char buf[128] = { 0 };
            char* p = buf;
            for (int i = 15; i >= 0; --i) {
                BYTE b = addr.s6_addr[i];
                int low = b & 0x0F;
                int high = (b >> 4) & 0x0F;
                p += snprintf(p, buf + sizeof(buf) - p, "%x.%x.", low, high);
            }
            snprintf(p, buf + sizeof(buf) - p, "ip6.arpa");
            return std::string(buf);
        }
        return ipStr;
    }
