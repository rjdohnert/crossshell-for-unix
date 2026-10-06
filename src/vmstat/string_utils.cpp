#include "string_utils.hpp"

std::string StringUtils::WStrToStr(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), NULL, 0, NULL, NULL);
        if (size <= 0) return "";
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()), &str[0], size, NULL, NULL);
        return str;
    }

bool StringUtils::TryParseInt(const std::string& text, int minValue, int& outValue) {
        if (text.empty()) return false;
        errno = 0;
        char* end = nullptr;
        long parsed = std::strtol(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0' || errno == ERANGE || parsed < minValue || parsed > INT_MAX) {
            return false;
        }
        outValue = static_cast<int>(parsed);
        return true;
    }

std::string StringUtils::FormatLongOrNA(bool available, long value, int width) {
        std::ostringstream ss;
        if (available) {
            ss << std::setw(width) << value;
        } else {
            ss << std::setw(width) << "N/A";
        }
        return ss.str();
    }

std::string StringUtils::FormatValueRaw(uint64_t bytes, UnitMode mode) {
        char buf[64];
        if (mode == UnitMode::Human) {
            const char* units[] = { "B", "K", "M", "G", "T" };
            double val = static_cast<double>(bytes);
            int idx = 0;
            while (val >= 1024.0 && idx < 4) {
                val /= 1024.0;
                idx++;
            }
            if (idx == 0) sprintf_s(buf, "%lluB", bytes);
            else sprintf_s(buf, "%.1f%s", val, units[idx]);
        } else if (mode == UnitMode::Megabytes) {
            sprintf_s(buf, "%llu", bytes / (1024 * 1024));
        } else {
            sprintf_s(buf, "%llu", bytes / 1024);
        }
        return buf;
    }

std::string StringUtils::FormatValue(uint64_t bytes, UnitMode mode, int width) {
        std::string raw = FormatValueRaw(bytes, mode);
        std::ostringstream ss;
        ss << std::setw(width) << raw;
        return ss.str();
    }
