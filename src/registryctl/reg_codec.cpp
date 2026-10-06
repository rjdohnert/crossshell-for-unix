#include "json.hpp"
#include "reg_codec.hpp"
#include "string_conversion.hpp"

HKEY RegCodec::ParseRootKey(const std::string& keyStr) {
        std::string upper = Utils::ToUpper(keyStr);
        if (upper == "HKLM" || upper == "HKEY_LOCAL_MACHINE")   return HKEY_LOCAL_MACHINE;
        if (upper == "HKCU" || upper == "HKEY_CURRENT_USER")    return HKEY_CURRENT_USER;
        if (upper == "HKCR" || upper == "HKEY_CLASSES_ROOT")    return HKEY_CLASSES_ROOT;
        if (upper == "HKU"  || upper == "HKEY_USERS")           return HKEY_USERS;
        if (upper == "HKCC" || upper == "HKEY_CURRENT_CONFIG")  return HKEY_CURRENT_CONFIG;
        return nullptr;
    }

bool RegCodec::SplitPath(const std::string& fullPath, HKEY& outRoot, std::string& outRootStr, std::string& outSubKey) {
        size_t idx = fullPath.find_first_of("\\/");
        if (idx == std::string::npos) {
            outRootStr = fullPath;
            outRoot = ParseRootKey(outRootStr);
            outSubKey = "";
            return outRoot != nullptr;
        }
        outRootStr = fullPath.substr(0, idx);
        outSubKey = fullPath.substr(idx + 1);
        outRoot = ParseRootKey(outRootStr);
        return outRoot != nullptr;
    }

DWORD RegCodec::ParseTypeString(const std::string& typeStr) {
        std::string u = Utils::ToUpper(typeStr);
        if (u == "REG_SZ" || u == "STRING" || u == "SZ") return REG_SZ;
        if (u == "REG_EXPAND_SZ" || u == "EXPAND_SZ" || u == "EXPAND") return REG_EXPAND_SZ;
        if (u == "REG_DWORD" || u == "DWORD" || u == "UINT32" || u == "INT") return REG_DWORD;
        if (u == "REG_QWORD" || u == "QWORD" || u == "UINT64") return REG_QWORD;
        if (u == "REG_MULTI_SZ" || u == "MULTI_SZ" || u == "STRINGS") return REG_MULTI_SZ;
        if (u == "REG_BINARY" || u == "BINARY" || u == "HEX") return REG_BINARY;
        return REG_NONE;
    }

bool RegCodec::EncodeData(DWORD type, const std::string& input, std::vector<uint8_t>& outBytes, std::string& err) {
        outBytes.clear();
        try {
            switch (type) {
                case REG_SZ:
                case REG_EXPAND_SZ: {
                    std::wstring ws = Utils::ToWString(input);
                    size_t bytes = (ws.length() + 1) * sizeof(wchar_t);
                    outBytes.resize(bytes);
                    memcpy(outBytes.data(), ws.c_str(), bytes);
                    return true;
                }
                case REG_DWORD: {
                    uint32_t val = 0;
                    if (input.rfind("0x", 0) == 0 || input.rfind("0X", 0) == 0) {
                        val = std::stoul(input, nullptr, 16);
                    } else {
                        val = std::stoul(input, nullptr, 10);
                    }
                    outBytes.resize(sizeof(uint32_t));
                    memcpy(outBytes.data(), &val, sizeof(uint32_t));
                    return true;
                }
                case REG_QWORD: {
                    uint64_t val = 0;
                    if (input.rfind("0x", 0) == 0 || input.rfind("0X", 0) == 0) {
                        val = std::stoull(input, nullptr, 16);
                    } else {
                        val = std::stoull(input, nullptr, 10);
                    }
                    outBytes.resize(sizeof(uint64_t));
                    memcpy(outBytes.data(), &val, sizeof(uint64_t));
                    return true;
                }
                case REG_MULTI_SZ: {
                    std::vector<std::wstring> lines;
                    std::string segment;
                    std::stringstream ss(input);
                    while (std::getline(ss, segment, ';')) {
                        if (!segment.empty()) lines.push_back(Utils::ToWString(segment));
                    }
                    size_t totalChars = 2; // Strict double-null baseline (\0\0)
                    for (auto& s : lines) totalChars += s.length() + 1;
                    outBytes.assign(totalChars * sizeof(wchar_t), 0);
                    wchar_t* dest = reinterpret_cast<wchar_t*>(outBytes.data());
                    for (auto& s : lines) {
                        memcpy(dest, s.c_str(), s.length() * sizeof(wchar_t));
                        dest += s.length() + 1;
                    }
                    return true;
                }
                case REG_BINARY: {
                    outBytes = Utils::HexToBytes(input);
                    return true;
                }
                default:
                    err = "Unsupported registry data type.";
                    return false;
            }
        } catch (const std::exception& e) {
            err = std::string("Data parsing exception: ") + e.what();
            return false;
        }
    }
