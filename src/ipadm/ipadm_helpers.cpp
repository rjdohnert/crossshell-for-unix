// String/encoding and interface-classification helpers.
#include "ipadm_helpers.h"

std::string WStringToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.length()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required <= 0) {
        return {};
    }

    std::string out(static_cast<size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.length()),
        &out[0],
        required,
        nullptr,
        nullptr);

    if (written <= 0) {
        return {};
    }

    return out;
}

// Escape a string for safe inclusion in JSON output.
std::string JsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// Escape a string for safe inclusion in CSV output.
std::string CsvEscape(const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

bool IsRealNetworkInterface(ULONG ifType, const std::string& friendlyName) {
    if (ifType == IF_TYPE_SOFTWARE_LOOPBACK) {
        return true;
    }

    switch (ifType) {
        case IF_TYPE_ETHERNET_CSMACD:
        case IF_TYPE_IEEE80211:
            break;
        default:
            return false;
    }

    return !IsSyntheticWindowsInterface(friendlyName, ifType);
}
