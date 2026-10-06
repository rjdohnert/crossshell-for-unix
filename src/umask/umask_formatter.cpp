#include "umask_formatter.hpp"

std::string UmaskFormatter::WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size_needed <= 0) return "";
        std::string out(static_cast<size_t>(size_needed - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, out.data(), size_needed, nullptr, nullptr);
        return out;
    }

std::string UmaskFormatter::FormatOctalMask(unsigned int mask) {
        std::stringstream ss;
        ss << std::setw(4) << std::setfill('0') << std::oct << (mask & MASK_ALL_BITS);
        return ss.str();
    }

std::string UmaskFormatter::FormatSymbolicMask(unsigned int mask) {
        unsigned int perm = (~mask) & MASK_ALL_BITS;

        auto formatWho = [](unsigned int p) -> std::string {
            std::string s;
            if (p & 4) s += "r";
            if (p & 2) s += "w";
            if (p & 1) s += "x";
            return s;
        };

        std::string u = formatWho((perm >> 6) & 7);
        std::string g = formatWho((perm >> 3) & 7);
        std::string o = formatWho(perm & 7);

        return "u=" + u + ",g=" + g + ",o=" + o;
    }
