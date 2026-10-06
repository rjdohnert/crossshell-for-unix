#include "registry_record.hpp"
#include "string_conversion.hpp"

std::string RegistryRecord::GetTypeString() const {
        switch (type) {
            case REG_SZ:        return "REG_SZ";
            case REG_EXPAND_SZ: return "REG_EXPAND_SZ";
            case REG_BINARY:    return "REG_BINARY";
            case REG_DWORD:     return "REG_DWORD";
            case REG_MULTI_SZ:  return "REG_MULTI_SZ";
            case REG_QWORD:     return "REG_QWORD";
            default:            return "REG_NONE";
        }
    }

std::string RegistryRecord::GetFormattedData() const {
        if (!exists) return "(non-existent)";
        if (rawData.empty()) return "";

        switch (type) {
            case REG_SZ:
            case REG_EXPAND_SZ: {
                std::wstring ws(reinterpret_cast<const wchar_t*>(rawData.data()), rawData.size() / sizeof(wchar_t));
                while (!ws.empty() && ws.back() == L'\0') ws.pop_back();
                return Utils::ToString(ws);
            }
            case REG_DWORD: {
                if (rawData.size() >= sizeof(DWORD)) {
                    DWORD val = *reinterpret_cast<const DWORD*>(rawData.data());
                    std::ostringstream ss;
                    ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << val << " (" << std::dec << val << ")";
                    return ss.str();
                }
                break;
            }
            case REG_QWORD: {
                if (rawData.size() >= sizeof(UINT64)) {
                    UINT64 val = *reinterpret_cast<const UINT64*>(rawData.data());
                    std::ostringstream ss;
                    ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << val << " (" << std::dec << val << ")";
                    return ss.str();
                }
                break;
            }
            case REG_MULTI_SZ: {
                std::vector<std::string> parts;
                const wchar_t* ptr = reinterpret_cast<const wchar_t*>(rawData.data());
                size_t cch = rawData.size() / sizeof(wchar_t);
                size_t cur = 0;
                while (cur < cch && ptr[cur] != L'\0') {
                    std::wstring sub(&ptr[cur]);
                    parts.push_back(Utils::ToString(sub));
                    cur += sub.length() + 1;
                }
                std::ostringstream ss;
                for (size_t i = 0; i < parts.size(); ++i) {
                    ss << parts[i] << (i + 1 < parts.size() ? "; " : "");
                }
                return ss.str();
            }
            case REG_BINARY: {
                std::ostringstream ss;
                for (size_t i = 0; i < rawData.size(); ++i) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(rawData[i]) << (i + 1 < rawData.size() ? " " : "");
                }
                return ss.str();
            }
        }
        return "<raw size: " + std::to_string(rawData.size()) + " bytes>";
    }
