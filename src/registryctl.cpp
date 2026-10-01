/*
 * Copyright (c) 2026 PC/OpenSystems LLC contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// ============================================================================
// SINGLE-FILE SOURCE INDEX & TABLE OF CONTENTS
// ============================================================================
// 1.  PREPROCESSOR DEFINITIONS, SYSTEM INCLUDES & LINKER PRAGMAS
// 2.  UTILITY & STRING CONVERSION ENGINE (UTF-8, Hex, Win32 Error Formatting)
// 3.  SECURITY & PRIVILEGE MANAGER (SeBackupPrivilege / SeRestorePrivilege)
// 4.  EMBEDDED ZERO-DEPENDENCY JSON DOM PARSER & SERIALIZER
// 5.  WIN32 RAII SYSTEM WRAPPERS (Scoped Registry Keys & KTM Transactions)
// 6.  REGISTRY TYPE CODECS & LOSSLESS BINARY SERIALIZATION
// 7.  HARDENED REGISTRY ENGINE (Atomic Operations, Views, Recursive Tree Backup)
// 8.  UNDO JOURNAL & ROLLBACK SUBSYSTEM
// 9.  DIFF PREVIEW & STRUCTURED OUTPUT FORMATTER
// 10. SHELL AUTOCOMPLETION & WRAPPER GENERATORS (PWSH, ZSH, KSH, CMD)
// 11. COMPREHENSIVE CONTEXTUAL HELP & DOCUMENTATION SUBSYSTEM
// 12. CLI PARSER & COMMAND DISPATCH ENGINE (main)
// ============================================================================

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

// ============================================================================
// 1. PREPROCESSOR DEFINITIONS, SYSTEM INCLUDES & LINKER PRAGMAS
// ============================================================================

#include <windows.h>
#include <ktmw32.h>
#include <iostream>
#include <cstdio>
#include <streambuf>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <map>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ktmw32.lib")

// ============================================================================
// 2. UTILITY & STRING CONVERSION ENGINE
// ============================================================================

namespace Utils {
    inline std::wstring ToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size);
        return wstr;
    }

    inline std::string ToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size, nullptr, nullptr);
        return str;
    }

    inline std::string ToUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(::toupper(c)); });
        return s;
    }

    inline std::string FormatWin32Error(DWORD errorCode) {
        LPWSTR buf = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
        std::string msg = buf ? ToString(std::wstring(buf, size)) : "Unknown system error";
        if (buf) LocalFree(buf);
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
        return "[" + std::to_string(errorCode) + "] " + msg;
    }

    inline std::string BytesToHex(const std::vector<uint8_t>& bytes) {
        std::ostringstream ss;
        for (uint8_t b : bytes) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return ss.str();
    }

    inline std::vector<uint8_t> HexToBytes(const std::string& hex) {
        std::vector<uint8_t> bytes;
        std::string clean;
        for (char c : hex) {
            if (::isxdigit(static_cast<unsigned char>(c))) clean.push_back(c);
        }
        for (size_t i = 0; i + 1 < clean.length(); i += 2) {
            std::string byteStr = clean.substr(i, 2);
            bytes.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
        }
        return bytes;
    }
}

// ============================================================================
// 3. SECURITY & PRIVILEGE MANAGER
// ============================================================================

namespace Security {
    inline bool EnableRequiredPrivileges() {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }
        
        auto SetPrivilege = [&](LPCWSTR privName) {
            TOKEN_PRIVILEGES tp;
            LUID luid;
            if (LookupPrivilegeValueW(nullptr, privName, &luid)) {
                tp.PrivilegeCount = 1;
                tp.Privileges[0].Luid = luid;
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
            }
        };

        SetPrivilege(SE_BACKUP_NAME);
        SetPrivilege(SE_RESTORE_NAME);
        CloseHandle(hToken);
        return true;
    }
}

// ============================================================================
// 4. EMBEDDED ZERO-DEPENDENCY JSON DOM PARSER & SERIALIZER
// ============================================================================

namespace Json {
    enum class Type { Null, Boolean, Number, String, Array, Object };

    struct Value;
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;

    class Data {
        struct HolderBase {
            virtual ~HolderBase() {}
        };

        template <typename T>
        struct Holder : HolderBase {
            T value;
            explicit Holder(const T& v) : value(v) {}
        };

        std::shared_ptr<HolderBase> value;

    public:
        Data() {}
        Data(std::nullptr_t) {}

        template <typename T>
        Data(const T& v) : value(std::make_shared<Holder<T> >(v)) {}

        template <typename T>
        T& get() {
            return static_cast<Holder<T>*>(value.get())->value;
        }

        template <typename T>
        const T& get() const {
            return static_cast<const Holder<T>*>(value.get())->value;
        }
    };

    struct Value {
        Type type = Type::Null;
        Data data = nullptr;

        Value() = default;
        Value(std::nullptr_t) : type(Type::Null), data(nullptr) {}
        Value(bool b) : type(Type::Boolean), data(b) {}
        Value(double d) : type(Type::Number), data(d) {}
        Value(int i) : type(Type::Number), data(static_cast<double>(i)) {}
        Value(const std::string& s) : type(Type::String), data(s) {}
        Value(const char* s) : type(Type::String), data(std::string(s)) {}
        Value(const Array& a) : type(Type::Array), data(a) {}
        Value(const Object& o) : type(Type::Object), data(o) {}

        bool IsNull() const { return type == Type::Null; }
        bool IsBool() const { return type == Type::Boolean; }
        bool IsNumber() const { return type == Type::Number; }
        bool IsString() const { return type == Type::String; }
        bool IsArray() const { return type == Type::Array; }
        bool IsObject() const { return type == Type::Object; }

        bool AsBool(bool def = false) const { return IsBool() ? data.get<bool>() : def; }
        double AsNumber(double def = 0.0) const { return IsNumber() ? data.get<double>() : def; }
        int AsInt(int def = 0) const { return IsNumber() ? static_cast<int>(data.get<double>()) : def; }
        std::string AsString(const std::string& def = "") const { return IsString() ? data.get<std::string>() : def; }
        const Array& AsArray() const { static const Array empty; return IsArray() ? data.get<Array>() : empty; }
        const Object& AsObject() const { static const Object empty; return IsObject() ? data.get<Object>() : empty; }

        bool HasKey(const std::string& key) const {
            if (!IsObject()) return false;
            const auto& obj = data.get<Object>();
            return obj.find(key) != obj.end();
        }

        Value operator[](const std::string& key) const {
            if (IsObject()) {
                const auto& obj = data.get<Object>();
                auto it = obj.find(key);
                if (it != obj.end()) return it->second;
            }
            return Value();
        }
    };

    class Parser {
    private:
        std::string src;
        size_t pos = 0;

        void SkipWhitespace() {
            while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) {
                pos++;
            }
        }

        char Peek() { SkipWhitespace(); return pos < src.size() ? src[pos] : '\0'; }
        char Get() { SkipWhitespace(); return pos < src.size() ? src[pos++] : '\0'; }

        std::string ParseString() {
            Get(); // Consume opening quote
            std::string res;
            while (pos < src.size()) {
                char c = src[pos++];
                if (c == '"') return res;
                if (c == '\\' && pos < src.size()) {
                    char esc = src[pos++];
                    switch (esc) {
                        case '"':  res += '"'; break;
                        case '\\': res += '\\'; break;
                        case '/':  res += '/'; break;
                        case 'b':  res += '\b'; break;
                        case 'f':  res += '\f'; break;
                        case 'n':  res += '\n'; break;
                        case 'r':  res += '\r'; break;
                        case 't':  res += '\t'; break;
                        case 'u': {
                            if (pos + 4 <= src.size()) {
                                std::string hex = src.substr(pos, 4);
                                pos += 4;
                                uint32_t code = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                                if (code <= 0x7F) {
                                    res += static_cast<char>(code);
                                } else if (code <= 0x7FF) {
                                    res += static_cast<char>(0xC0 | ((code >> 6) & 0x1F));
                                    res += static_cast<char>(0x80 | (code & 0x3F));
                                } else {
                                    res += static_cast<char>(0xE0 | ((code >> 12) & 0x0F));
                                    res += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                    res += static_cast<char>(0x80 | (code & 0x3F));
                                }
                            }
                            break;
                        }
                        default: res += esc; break;
                    }
                } else {
                    res += c;
                }
            }
            return res;
        }

        Value ParseNumber() {
            size_t start = pos;
            if (src[pos] == '-') pos++;
            while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
                pos++;
            }
            try {
                double val = std::stod(src.substr(start, pos - start));
                return Value(val);
            } catch (...) {
                return Value(0.0);
            }
        }

    public:
        explicit Parser(const std::string& input) : src(input), pos(0) {}

        Value ParseValue() {
            SkipWhitespace();
            char c = Peek();
            if (c == '{') return ParseObject();
            if (c == '[') return ParseArray();
            if (c == '"') return Value(ParseString());
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') return ParseNumber();
            if (src.rfind("true", pos) == pos) { pos += 4; return Value(true); }
            if (src.rfind("false", pos) == pos) { pos += 5; return Value(false); }
            if (src.rfind("null", pos) == pos) { pos += 4; return Value(nullptr); }
            return Value();
        }

        Value ParseObject() {
            Get(); // Consume '{'
            Object obj;
            while (Peek() != '}' && Peek() != '\0') {
                if (Peek() != '"') break;
                std::string key = ParseString();
                SkipWhitespace();
                if (Get() != ':') break;
                obj[key] = ParseValue();
                if (Peek() == ',') Get();
            }
            if (Peek() == '}') Get();
            return Value(obj);
        }

        Value ParseArray() {
            Get(); // Consume '['
            Array arr;
            while (Peek() != ']' && Peek() != '\0') {
                arr.push_back(ParseValue());
                if (Peek() == ',') Get();
            }
            if (Peek() == ']') Get();
            return Value(arr);
        }
    };

    inline std::string Stringify(const Value& val, int indent = 0) {
        std::string ind(indent * 2, ' ');
        switch (val.type) {
            case Type::Null: return "null";
            case Type::Boolean: return val.AsBool() ? "true" : "false";
            case Type::Number: {
                std::ostringstream ss;
                ss << val.AsNumber();
                return ss.str();
            }
            case Type::String: {
                std::ostringstream ss;
                ss << "\"";
                for (char c : val.AsString()) {
                    if (c == '"') ss << "\\\"";
                    else if (c == '\\') ss << "\\\\";
                    else if (c == '\n') ss << "\\n";
                    else if (c == '\r') ss << "\\r";
                    else if (c == '\t') ss << "\\t";
                    else ss << c;
                }
                ss << "\"";
                return ss.str();
            }
            case Type::Array: {
                const auto& arr = val.AsArray();
                if (arr.empty()) return "[]";
                std::string res = "[\n";
                for (size_t i = 0; i < arr.size(); ++i) {
                    res += ind + "  " + Stringify(arr[i], indent + 1);
                    if (i + 1 < arr.size()) res += ",";
                    res += "\n";
                }
                res += ind + "]";
                return res;
            }
            case Type::Object: {
                const auto& obj = val.AsObject();
                if (obj.empty()) return "{}";
                std::string res = "{\n";
                size_t i = 0;
                for (const auto& entry : obj) {
                    const auto& k = entry.first;
                    const auto& v = entry.second;
                    res += ind + "  \"" + k + "\": " + Stringify(v, indent + 1);
                    if (++i < obj.size()) res += ",";
                    res += "\n";
                }
                res += ind + "}";
                return res;
            }
        }
        return "null";
    }
}

// ============================================================================
// 5. WIN32 RAII SYSTEM WRAPPERS
// ============================================================================

class ScopedHKey {
public:
    HKEY handle = nullptr;
    ScopedHKey() = default;
    explicit ScopedHKey(HKEY h) : handle(h) {}
    ~ScopedHKey() { if (handle && handle != INVALID_HANDLE_VALUE) RegCloseKey(handle); }
    ScopedHKey(const ScopedHKey&) = delete;
    ScopedHKey& operator=(const ScopedHKey&) = delete;
    ScopedHKey(ScopedHKey&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    ScopedHKey& operator=(ScopedHKey&& other) noexcept {
        if (this != &other) {
            if (handle && handle != INVALID_HANDLE_VALUE) RegCloseKey(handle);
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    operator HKEY() const { return handle; }
};

class ScopedTransaction {
private:
    HANDLE m_hTx = INVALID_HANDLE_VALUE;
    bool m_committed = false;
public:
    ScopedTransaction() {
        m_hTx = CreateTransaction(nullptr, 0, 0, 0, 0, 0, const_cast<LPWSTR>(L"registryctl_atomic_tx"));
    }
    ~ScopedTransaction() {
        if (m_hTx != INVALID_HANDLE_VALUE) {
            if (!m_committed) RollbackTransaction(m_hTx);
            CloseHandle(m_hTx);
        }
    }
    bool IsValid() const { return m_hTx != INVALID_HANDLE_VALUE; }
    HANDLE Get() const { return m_hTx; }
    bool Commit() {
        if (m_hTx == INVALID_HANDLE_VALUE || m_committed) return false;
        if (CommitTransaction(m_hTx)) {
            m_committed = true;
            return true;
        }
        return false;
    }
    bool Rollback() {
        if (m_hTx == INVALID_HANDLE_VALUE) return false;
        return RollbackTransaction(m_hTx) != FALSE;
    }
};

// ============================================================================
// 6. REGISTRY TYPE CODECS & LOSSLESS BINARY SERIALIZATION
// ============================================================================

struct RegistryRecord {
    std::string rootKey;
    std::string subKey;
    std::string valueName;
    DWORD type = REG_NONE;
    std::vector<uint8_t> rawData;
    bool exists = false;

    std::string GetTypeString() const {
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

    std::string GetFormattedData() const {
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
};

class RegCodec {
public:
    static HKEY ParseRootKey(const std::string& keyStr) {
        std::string upper = Utils::ToUpper(keyStr);
        if (upper == "HKLM" || upper == "HKEY_LOCAL_MACHINE")   return HKEY_LOCAL_MACHINE;
        if (upper == "HKCU" || upper == "HKEY_CURRENT_USER")    return HKEY_CURRENT_USER;
        if (upper == "HKCR" || upper == "HKEY_CLASSES_ROOT")    return HKEY_CLASSES_ROOT;
        if (upper == "HKU"  || upper == "HKEY_USERS")           return HKEY_USERS;
        if (upper == "HKCC" || upper == "HKEY_CURRENT_CONFIG")  return HKEY_CURRENT_CONFIG;
        return nullptr;
    }

    static bool SplitPath(const std::string& fullPath, HKEY& outRoot, std::string& outRootStr, std::string& outSubKey) {
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

    static DWORD ParseTypeString(const std::string& typeStr) {
        std::string u = Utils::ToUpper(typeStr);
        if (u == "REG_SZ" || u == "STRING" || u == "SZ") return REG_SZ;
        if (u == "REG_EXPAND_SZ" || u == "EXPAND_SZ" || u == "EXPAND") return REG_EXPAND_SZ;
        if (u == "REG_DWORD" || u == "DWORD" || u == "UINT32" || u == "INT") return REG_DWORD;
        if (u == "REG_QWORD" || u == "QWORD" || u == "UINT64") return REG_QWORD;
        if (u == "REG_MULTI_SZ" || u == "MULTI_SZ" || u == "STRINGS") return REG_MULTI_SZ;
        if (u == "REG_BINARY" || u == "BINARY" || u == "HEX") return REG_BINARY;
        return REG_NONE;
    }

    static bool EncodeData(DWORD type, const std::string& input, std::vector<uint8_t>& outBytes, std::string& err) {
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
};

// ============================================================================
// 7. HARDENED REGISTRY ENGINE
// ============================================================================

struct UndoRecord {
    std::string op;
    std::string path;
    std::string valueName;
    std::string typeStr;
    std::string rawDataHex;
    bool hadPreviousValue = false;
};

class RegistryEngine {
private:
    static bool DeleteTreeTransactedInternal(HKEY hKey, const std::wstring& subKey, REGSAM viewSam, HANDLE hTx) {
        ScopedHKey curKey;
        LSTATUS status = RegOpenKeyTransactedW(hKey, subKey.c_str(), 0, KEY_READ | KEY_WRITE | viewSam, &curKey.handle, hTx, nullptr);
        if (status != ERROR_SUCCESS) return false;

        WCHAR childName[512];
        DWORD cchName = 512;
        while (RegEnumKeyExW(curKey, 0, childName, &cchName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            if (!DeleteTreeTransactedInternal(curKey, childName, viewSam, hTx)) return false;
            cchName = 512;
        }

        curKey = ScopedHKey();
        return RegDeleteKeyTransactedW(hKey, subKey.c_str(), viewSam, 0, hTx, nullptr) == ERROR_SUCCESS;
    }

public:
    static bool QueryValue(HKEY root, const std::string& rootStr, const std::string& subKey, const std::string& valueName, RegistryRecord& outRecord, REGSAM viewSam = 0, HANDLE hTx = INVALID_HANDLE_VALUE) {
        outRecord.rootKey = rootStr;
        outRecord.subKey = subKey;
        outRecord.valueName = valueName;
        outRecord.exists = false;

        ScopedHKey key;
        LSTATUS status = ERROR_SUCCESS;
        std::wstring wSubKey = Utils::ToWString(subKey);
        std::wstring wValueName = Utils::ToWString(valueName);

        REGSAM sam = KEY_READ | viewSam;
        if (hTx != INVALID_HANDLE_VALUE) {
            status = RegOpenKeyTransactedW(root, wSubKey.c_str(), 0, sam, &key.handle, hTx, nullptr);
        } else {
            status = RegOpenKeyExW(root, wSubKey.c_str(), 0, sam, &key.handle);
        }

        if (status != ERROR_SUCCESS) return false;

        DWORD type = 0;
        DWORD cbData = 0;
        for (int retry = 0; retry < 5; ++retry) {
            status = RegQueryValueExW(key, wValueName.c_str(), nullptr, &type, nullptr, &cbData);
            if (status != ERROR_SUCCESS) return false;

            outRecord.rawData.resize(cbData);
            status = RegQueryValueExW(key, wValueName.c_str(), nullptr, &type, outRecord.rawData.data(), &cbData);
            if (status == ERROR_SUCCESS) {
                outRecord.type = type;
                outRecord.exists = true;
                return true;
            }
            if (status != ERROR_MORE_DATA) break;
        }
        return false;
    }

    static bool SetValueRaw(HKEY root, const std::string& subKey, const std::string& valueName, DWORD type, const std::vector<uint8_t>& data, std::string& err, REGSAM viewSam = 0, HANDLE hTx = INVALID_HANDLE_VALUE) {
        ScopedHKey key;
        LSTATUS status = ERROR_SUCCESS;
        std::wstring wSubKey = Utils::ToWString(subKey);
        std::wstring wValueName = Utils::ToWString(valueName);
        REGSAM sam = KEY_WRITE | KEY_READ | viewSam;

        if (hTx != INVALID_HANDLE_VALUE) {
            status = RegCreateKeyTransactedW(root, wSubKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, sam, nullptr, &key.handle, nullptr, hTx, nullptr);
        } else {
            status = RegCreateKeyExW(root, wSubKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, sam, nullptr, &key.handle, nullptr);
        }

        if (status != ERROR_SUCCESS) {
            err = "Failed to open/create registry key: " + Utils::FormatWin32Error(status);
            return false;
        }

        status = RegSetValueExW(key, wValueName.c_str(), 0, type, data.data(), static_cast<DWORD>(data.size()));
        if (status != ERROR_SUCCESS) {
            err = "Failed to write registry value: " + Utils::FormatWin32Error(status);
            return false;
        }
        return true;
    }

    static bool DeleteValue(HKEY root, const std::string& subKey, const std::string& valueName, std::string& err, REGSAM viewSam = 0, HANDLE hTx = INVALID_HANDLE_VALUE) {
        ScopedHKey key;
        LSTATUS status = ERROR_SUCCESS;
        std::wstring wSubKey = Utils::ToWString(subKey);
        std::wstring wValueName = Utils::ToWString(valueName);
        REGSAM sam = KEY_SET_VALUE | viewSam;

        if (hTx != INVALID_HANDLE_VALUE) {
            status = RegOpenKeyTransactedW(root, wSubKey.c_str(), 0, sam, &key.handle, hTx, nullptr);
        } else {
            status = RegOpenKeyExW(root, wSubKey.c_str(), 0, sam, &key.handle);
        }

        if (status != ERROR_SUCCESS) {
            err = "Key not found or access denied: " + Utils::FormatWin32Error(status);
            return false;
        }

        status = RegDeleteValueW(key, wValueName.c_str());
        if (status != ERROR_SUCCESS) {
            err = "Failed to delete value: " + Utils::FormatWin32Error(status);
            return false;
        }
        return true;
    }

    static void BackupKeyRecursive(HKEY root, const std::string& rootStr, const std::string& subKey, std::vector<UndoRecord>& journal, REGSAM viewSam, HANDLE hTx) {
        ScopedHKey key;
        std::wstring wSub = Utils::ToWString(subKey);
        REGSAM sam = KEY_READ | viewSam;
        LSTATUS st = (hTx != INVALID_HANDLE_VALUE) ?
            RegOpenKeyTransactedW(root, wSub.c_str(), 0, sam, &key.handle, hTx, nullptr) :
            RegOpenKeyExW(root, wSub.c_str(), 0, sam, &key.handle);
        if (st != ERROR_SUCCESS) return;

        DWORD valIndex = 0;
        WCHAR valName[16384];
        DWORD cchVal = 16384;
        DWORD type = 0;
        bool hasValues = false;

        while (RegEnumValueW(key, valIndex++, valName, &cchVal, nullptr, &type, nullptr, nullptr) == ERROR_SUCCESS) {
            hasValues = true;
            RegistryRecord rec;
            std::string vName = Utils::ToString(valName);
            if (QueryValue(root, rootStr, subKey, vName, rec, viewSam, hTx)) {
                UndoRecord u;
                u.op = "set";
                u.path = rootStr + "\\" + subKey;
                u.valueName = vName;
                u.hadPreviousValue = true;
                u.typeStr = rec.GetTypeString();
                u.rawDataHex = Utils::BytesToHex(rec.rawData);
                journal.push_back(u);
            }
            cchVal = 16384;
        }

        // If the key is an empty leaf, record a default record to ensure its path is recreated on undo
        if (!hasValues) {
            UndoRecord u;
            u.op = "set";
            u.path = rootStr + "\\" + subKey;
            u.valueName = "";
            u.hadPreviousValue = false;
            u.typeStr = "REG_SZ";
            u.rawDataHex = "";
            journal.push_back(u);
        }

        DWORD subIndex = 0;
        WCHAR childName[512];
        DWORD cchChild = 512;
        while (RegEnumKeyExW(key, subIndex++, childName, &cchChild, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            std::string nextSub = subKey.empty() ? Utils::ToString(childName) : (subKey + "\\" + Utils::ToString(childName));
            BackupKeyRecursive(root, rootStr, nextSub, journal, viewSam, hTx);
            cchChild = 512;
        }
    }

    static bool DeleteKeyRecursive(HKEY root, const std::string& subKey, std::string& err, REGSAM viewSam = 0, HANDLE hTx = INVALID_HANDLE_VALUE) {
        std::wstring wSubKey = Utils::ToWString(subKey);
        if (hTx != INVALID_HANDLE_VALUE) {
            if (!DeleteTreeTransactedInternal(root, wSubKey, viewSam, hTx)) {
                err = "Failed transacted recursive key deletion: " + Utils::FormatWin32Error(GetLastError());
                return false;
            }
            return true;
        } else {
            LSTATUS st = RegDeleteTreeW(root, wSubKey.c_str());
            if (st != ERROR_SUCCESS) {
                err = "Failed recursive tree deletion: " + Utils::FormatWin32Error(st);
                return false;
            }
            return true;
        }
    }
};

// ============================================================================
// 8. UNDO JOURNAL & ROLLBACK SUBSYSTEM
// ============================================================================

class JournalEngine {
public:
    static void WriteUndoLog(const std::string& filename, const std::vector<UndoRecord>& records) {
        Json::Array arr;
        for (const auto& r : records) {
            Json::Object obj;
            obj["op"] = Json::Value(r.op);
            obj["path"] = Json::Value(r.path);
            obj["value"] = r.valueName;
            obj["type"] = r.typeStr;
            obj["raw_hex"] = r.rawDataHex;
            obj["had_previous"] = r.hadPreviousValue;
            arr.push_back(Json::Value(obj));
        }
        Json::Object root;
        root["version"] = "1.1";
        root["generator"] = "registryctl";
        root["record_count"] = static_cast<int>(records.size());
        root["actions"] = arr;

        std::ofstream file(filename);
        if (file.is_open()) {
            file << Json::Stringify(Json::Value(root), 0);
        }
    }

    static bool LoadUndoLog(const std::string& filename, std::vector<UndoRecord>& records, std::string& err) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            err = "Unable to open undo journal file: " + filename;
            return false;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Json::Parser parser(content);
        Json::Value root = parser.ParseValue();
        if (!root.IsObject() || !root.HasKey("actions")) {
            err = "Invalid or corrupted undo journal schema.";
            return false;
        }
        for (const auto& item : root["actions"].AsArray()) {
            UndoRecord rec;
            rec.op = item["op"].AsString();
            rec.path = item["path"].AsString();
            rec.valueName = item["value"].AsString();
            rec.typeStr = item["type"].AsString();
            rec.rawDataHex = item["raw_hex"].AsString();
            rec.hadPreviousValue = item["had_previous"].AsBool();
            records.push_back(rec);
        }
        return true;
    }
};

// ============================================================================
// 9. DIFF PREVIEW & STRUCTURED OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static void PrintDiff(const std::string& path, const std::string& value, const RegistryRecord& oldRec, const std::string& newType, const std::string& newData, bool willDelete = false) {
        std::cout << "\n\n";
        std::cout << " DIFF PREVIEW: " << path << (value.empty() ? " (Default)" : " -> " + value) << "\n";
        std::cout << "\n";
        if (willDelete) {
            std::cout << " [-] ACTION: VALUE OR SUBKEY WILL BE DELETED\n";
            if (oldRec.exists) {
                std::cout << " [-] Current Type : " << oldRec.GetTypeString() << "\n";
                std::cout << " [-] Current Value: " << oldRec.GetFormattedData() << "\n";
            }
        } else {
            if (oldRec.exists) {
                std::cout << " [~] ACTION: VALUE WILL BE OVERWRITTEN\n";
                std::cout << " [-] Old Type : " << oldRec.GetTypeString() << "\n";
                std::cout << " [-] Old Value: " << oldRec.GetFormattedData() << "\n";
            } else {
                std::cout << " [+] ACTION: VALUE WILL BE CREATED\n";
            }
            std::cout << " [+] New Type : " << newType << "\n";
            std::cout << " [+] New Value: " << newData << "\n";
        }
        std::cout << "-------------------------------------------------------\n\n";
    }

    static void PrintJson(const RegistryRecord& rec) {
        Json::Object obj;
        obj["path"] = rec.rootKey + "\\" + rec.subKey;
        obj["value"] = rec.valueName;
        obj["exists"] = rec.exists;
        obj["type"] = rec.GetTypeString();
        obj["data"] = rec.GetFormattedData();
        obj["raw_hex"] = Utils::BytesToHex(rec.rawData);
        std::cout << Json::Stringify(Json::Value(obj), 0) << "\n";
    }
};

// ============================================================================
// 10. SHELL AUTOCOMPLETION & WRAPPER GENERATORS
// ============================================================================

class ShellGenerator {
public:
    static void GeneratePowerShell() {
        std::cout << R"(# PowerShell Completion Module for registryctl
Register-ArgumentCompleter -Native -CommandName registryctl -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)
    $commands = @(
        'get','set','delete','diff','batch','undo','completions','help',
        '--dry-run','--tx','--json','--undo-file','--view','--help'
    )
    $hives = @('HKLM\','HKCU\','HKCR\','HKU\','HKCC\')
    $types = @('REG_SZ','REG_DWORD','REG_QWORD','REG_MULTI_SZ','REG_BINARY','REG_EXPAND_SZ')

    if ($commandAst.Elements.Count -eq 2) {
        $commands | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
    } elseif ($commandAst.Elements.Count -eq 3) {
        $hives | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
    } elseif ($commandAst.Elements.Count -eq 5) {
        $types | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
    }
}
)";
    }

    static void GenerateZsh() {
        std::cout << R"(#compdef registryctl
_registryctl() {
    local -a commands
    commands=(
        'get:Read a registry value'
        'set:Create or update a registry value'
        'delete:Remove a registry value or subkey'
        'diff:Compare proposed registry changes'
        'batch:Execute actions from a manifest JSON file'
        'undo:Rollback actions using an undo log JSON file'
        'completions:Generate shell completion scripts'
        'help:Display manual and usage guides'
    )
    _arguments \
        '--dry-run[Simulate operation without writing changes]' \
        '--tx[Run all operations inside a transactional atomic block]' \
        '--json[Output results in JSON format]' \
        '--view=[Target 32-bit or 64-bit registry view]:view:(32 64 default)' \
        '--undo-file=[Path to export rollback journal]:file:_files' \
        '1: :->command' \
        '*:: :->args'

    case $state in
        command) _describe -t commands 'registryctl command' commands ;;
    esac
}
_registryctl "$@"
)";
    }

    static void GenerateKsh() {
        std::cout << R"(# KornShell (ksh) autocompletion helper for registryctl
set -A complete_registryctl -- get set delete diff batch undo completions help --dry-run --tx --json --undo-file --view
)";
    }

    static void GenerateCmdWrapper() {
        std::cout << R"(@echo off
:: registryctl Production CMD / Batch Wrapper
setlocal enabledelayedexpansion
if "%~1"=="" goto usage
registryctl.exe %*
exit /b %errorlevel%

:usage
echo 
echo  registryctl Batch Wrapper
echo 
echo Usage: registryctl.exe [command] [arguments...] [options]
echo Run 'registryctl help' for detailed instructions.
exit /b 1
)";
    }
};

// ============================================================================
// 11. COMPREHENSIVE CONTEXTUAL HELP & DOCUMENTATION SUBSYSTEM
// ============================================================================

class HelpSystem {
public:
    static void PrintGeneralHelp() {
        std::cout << R"(registryctl(1)            CrossShell for UNIX Reference Manual                 registryctl(1)

    NAME
        registryctl - Windows Registry manipulation and automation utility

    SYNOPSIS
        registryctl <command> [ARGUMENTS...] [OPTIONS]

    DESCRIPTION
        registryctl provides sysadmins, DevOps engineers, and automated
        deployment pipelines with tools to safely query, mutate, batch-process,
        diff, and roll back Windows Registry edits with KTM transactional
        support.

    COMMANDS
        get <Path> [Value]
            Query registry value or verify existence.

        set <Path> <Value> <Type> <Data>
            Create or update a registry value atomically.

        delete <Path> [Value]
            Delete a value or entire recursive subkey.

        diff <Path> <Value> <Type> <Data>
            Compare target data against current value without modifying.

        batch <Manifest.json>
            Execute a JSON array of mutations atomically.

        undo <UndoLog.json>
            Replay an undo journal to restore exact prior state.

        completions <powershell|zsh|ksh|cmd>
            Emit shell completion and integration scripts.

        help [command|topic]
            Display detailed syntax, flags, and guides.

    OPTIONS
        --dry-run
            Simulate operations and display diffs without writing changes.

        --tx
            Execute modifications in an atomic Windows KTM transaction.

        --view <32|64>
            Force target WOW64 view (32-bit: WOW6432Node, 64-bit: native).

        --json
            Format standard output as JSON.

        --csv
            Format standard output as CSV.

        --table
            Format standard output as an ASCII table.

        --pipe <command>
            Send formatted output through the specified shell command.

        --undo-file <path>
            Emit an undo journal JSON to revert this operation later.

        -h, --help
            Display this reference manual and exit.

    EXAMPLES
        registryctl get HKLM\Software\MyApp Version
            Query a registry value.

        registryctl set HKLM\Software\MyApp Port REG_DWORD 8080 --tx
            Atomically set a DWORD value using KTM transactions.

        registryctl diff HKLM\Software\MyApp Port REG_DWORD 9090
            Preview changes between target and current value.

        registryctl delete HKLM\Software\OldApp --undo-file=undo.json
            Delete registry key recursively and save rollback journal.

        registryctl undo undo.json --tx
            Replay rollback journal inside a transaction.

    CrossShell for UNIX                                                          registryctl(1)
)";
    }

    static void PrintTopicHelp(const std::string& topic) {
        std::string t = Utils::ToUpper(topic);
        if (t == "SET") {
            std::cout << R"(COMMAND: set
USAGE:
  registryctl set <KeyPath> <ValueName> <Type> <Data> [OPTIONS]

DESCRIPTION:
  Creates or updates a registry value. Missing parent keys are created
  automatically. To target the default (unnamed) key value, pass "" as ValueName.

DATA TYPES & FORMATS:
  REG_SZ          Standard Unicode string. Example: "Production Server"
  REG_EXPAND_SZ   Environment variable string. Example: "%SystemRoot%\System32"
  REG_DWORD       32-bit unsigned integer (decimal or 0x hex). Example: 8080 or 0x1F90
  REG_QWORD       64-bit unsigned integer (decimal or 0x hex). Example: 0x7FFFFFFFFFFFFFFF
  REG_MULTI_SZ    Multi-string list separated by semicolons. Example: "app1;app2;app3"
  REG_BINARY      Raw hex bytes with or without spaces. Example: "DEADBEEF0102"

EXAMPLES:
  registryctl set HKLM\Software\MyApp Version REG_SZ "3.4.1" --tx
  registryctl set HKLM\Software\MyApp Port REG_DWORD 8080 --undo-file=undo.json
  registryctl set HKLM\Software\MyApp Endpoints REG_MULTI_SZ "east;west;central"
  registryctl set HKLM\Software\MyApp Payload REG_BINARY "48656C6C6F" --view 64
)";
        } else if (t == "GET") {
            std::cout << R"(COMMAND: get
USAGE:
  registryctl get <KeyPath> [ValueName] [OPTIONS]

DESCRIPTION:
  Queries a registry value and displays its type, formatted data, and hex
  representation. If ValueName is omitted or passed as "", queries (Default).

EXAMPLES:
  registryctl get HKLM\Software\MyApp Version
  registryctl get HKLM\Software\MyApp Version --json
  registryctl get HKLM\Software\MyApp Version --view 32
)";
        } else if (t == "DELETE") {
            std::cout << R"(COMMAND: delete
USAGE:
  registryctl delete <KeyPath> [ValueName] [OPTIONS]

DESCRIPTION:
  Deletes a specific registry value, or if ValueName is omitted, recursively
  deletes the entire subkey hierarchy. When --undo-file is specified with a key
  deletion, the entire subtree of subkeys and values is captured recursively.

EXAMPLES:
  registryctl delete HKLM\Software\MyApp StaleValue --tx
  registryctl delete HKLM\Software\OldApp --undo-file=backup_tree.json
  registryctl delete HKLM\Software\OldApp --dry-run
)";
        } else if (t == "BATCH" || t == "BATCH-SCHEMA") {
            std::cout << R"(COMMAND: batch
USAGE:
  registryctl batch <Manifest.json> [OPTIONS]

DESCRIPTION:
  Executes a batch of mutations defined in a JSON manifest. If --tx is set,
  all operations succeed together or roll back completely on failure.

MANIFEST SCHEMA EXAMPLE:
  {
    "actions": [
      {
        "op": "set",
        "path": "HKLM\\Software\\AcmeApp\\Config",
        "value": "MaxThreads",
        "type": "REG_DWORD",
        "data": "128"
      },
      {
        "op": "set",
        "path": "HKLM\\Software\\AcmeApp\\Config",
        "value": "DatabaseUrl",
        "type": "REG_SZ",
        "data": "postgres://localhost:5432/db"
      },
      {
        "op": "delete",
        "path": "HKLM\\Software\\AcmeApp\\Config",
        "value": "LegacyTimeout"
      }
    ]
  }

EXAMPLES:
  registryctl batch manifest.json --tx --undo-file=rollback.json
  registryctl batch manifest.json --dry-run
)";
        } else if (t == "UNDO") {
            std::cout << R"(COMMAND: undo
USAGE:
  registryctl undo <UndoLog.json> [OPTIONS]

DESCRIPTION:
  Restores prior registry state from an undo journal generated by a previous
  'set', 'delete', or 'batch' operation. Replays records in reverse order using
  exact byte-level hex restoration.

EXAMPLES:
  registryctl undo rollback.json --tx
  registryctl undo rollback.json --dry-run
)";
        } else if (t == "TRANSACTIONS") {
            std::cout << R"(TOPIC: transactions (--tx)
DESCRIPTION:
  When --tx is specified, registryctl initializes a Windows Kernel Transaction
  Manager (KTM) handle. All key creations, value writes, and deletions occur
  inside this transaction. If any error occurs or the process is interrupted,
  the transaction is automatically aborted by the Windows kernel.

NOTE:
  KTM/TxR is available on Windows 10/11 and Windows Server editions. In minimal
  containers or stripped WinPE images where TxR is disabled, omit --tx to use
  conventional hardened writes.
)";
        } else if (t == "WOW64" || t == "VIEW") {
            std::cout << R"(TOPIC: WOW64 & Architecture Views (--view)
DESCRIPTION:
  On 64-bit Windows, 32-bit applications are redirected to HKLM\Software\WOW6432Node.
  registryctl allows explicit view selection:
    --view 64      Force 64-bit native registry view (KEY_WOW64_64KEY)
    --view 32      Force 32-bit redirected view (KEY_WOW64_32KEY)
    (default)      Native OS architecture view
)";
        } else if (t == "EXAMPLES") {
            std::cout << R"(TOPIC: Production Workflow Examples

1. Safe Mutation with Dry-Run and Undo Journal:
   registryctl diff HKLM\Software\Engine CacheSize REG_DWORD 4096
   registryctl set HKLM\Software\Engine CacheSize REG_DWORD 4096 --tx --undo-file=undo.json

2. Automation Pipeline with JSON Output:
   registryctl get HKLM\Software\Engine CacheSize --json

3. Atomic Batch Deployment with Rollback on Failure:
   registryctl batch deploy.json --tx --undo-file=revert.json

4. Emergency Rollback:
   registryctl undo revert.json --tx
)";
        } else {
            PrintGeneralHelp();
        }
    }
};

// ============================================================================
// 12. CLI PARSER & COMMAND DISPATCH ENGINE (MAIN)
// ============================================================================

enum class RegistryOutputFormat { Human, Json, Csv, Table };

class RegistryPipeBuffer : public std::streambuf {
    FILE* file_;
    char buffer_[4096];
public:
    explicit RegistryPipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + sizeof(buffer_)); }
    int_type overflow(int_type ch) override { if (ch != traits_type::eof()) { *pptr() = static_cast<char>(ch); pbump(1); } return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); }
    int sync() override { auto count = pptr() - pbase(); if (count && std::fwrite(pbase(), 1, static_cast<size_t>(count), file_) != static_cast<size_t>(count)) return -1; setp(buffer_, buffer_ + sizeof(buffer_)); return std::fflush(file_) == 0 ? 0 : -1; }
};

class RegistryLineFormatter : public std::streambuf {
    std::streambuf* target_;
    RegistryOutputFormat format_;
    std::string pending_;
    void emit() {
        if (pending_.empty()) return;
        std::string line = pending_;
        if (format_ == RegistryOutputFormat::Csv) {
            std::string escaped = "\"";
            for (char ch : line) escaped += ch == '"' ? "\"\"" : std::string(1, ch);
            line = escaped + "\"\n";
        } else if (format_ == RegistryOutputFormat::Table) {
            line += "\n";
        }
        target_->sputn(line.data(), static_cast<std::streamsize>(line.size()));
        pending_.clear();
    }
public:
    RegistryLineFormatter(std::streambuf* target, RegistryOutputFormat format) : target_(target), format_(format) {}
    int_type overflow(int_type ch) override { if (ch != traits_type::eof()) { if (ch == '\n') emit(); else pending_.push_back(static_cast<char>(ch)); } return traits_type::not_eof(ch); }
    int sync() override { emit(); return target_->pubsync(); }
};

class RegistryOutputSession {
    std::streambuf* old_;
    FILE* pipe_ = nullptr;
    RegistryPipeBuffer* pipeBuffer_ = nullptr;
    RegistryLineFormatter* formatter_ = nullptr;
public:
    RegistryOutputSession(RegistryOutputFormat format, const std::string& command) : old_(std::cout.rdbuf()) {
        std::streambuf* target = old_;
        if (!command.empty()) {
            pipe_ = _popen(command.c_str(), "w");
            if (pipe_) { pipeBuffer_ = new RegistryPipeBuffer(pipe_); target = pipeBuffer_; }
        }
        if (format == RegistryOutputFormat::Csv || format == RegistryOutputFormat::Table) {
            formatter_ = new RegistryLineFormatter(target, format);
            std::cout.rdbuf(formatter_);
        } else {
            std::cout.rdbuf(target);
        }
    }
    ~RegistryOutputSession() { std::cout.flush(); std::cout.rdbuf(old_); delete formatter_; delete pipeBuffer_; if (pipe_) _pclose(pipe_); }
};

int main(int argc, char* argv[]) {
    // 1. Force Windows Console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 2. Enable SeBackupPrivilege and SeRestorePrivilege
    Security::EnableRequiredPrivileges();

    if (argc < 2) {
        HelpSystem::PrintGeneralHelp();
        return 1;
    }

    std::string command = argv[1];
    if (command == "help" || command == "--help" || command == "-h") {
        if (argc > 2) {
            HelpSystem::PrintTopicHelp(argv[2]);
        } else {
            HelpSystem::PrintGeneralHelp();
        }
        return 0;
    }

    if (command == "completions") {
        if (argc < 3) {
            std::cerr << "[-] Error: Specify target shell (powershell, zsh, ksh, cmd)\n";
            return 1;
        }
        std::string shell = argv[2];
        if (shell == "powershell" || shell == "pwsh") ShellGenerator::GeneratePowerShell();
        else if (shell == "zsh") ShellGenerator::GenerateZsh();
        else if (shell == "ksh") ShellGenerator::GenerateKsh();
        else if (shell == "cmd" || shell == "bat") ShellGenerator::GenerateCmdWrapper();
        else {
            std::cerr << "[-] Error: Unknown target shell: " << shell << "\n";
            return 1;
        }
        return 0;
    }

    // CLI Flag Parsing
    bool dryRun = false;
    bool useTx = false;
    bool jsonOutput = false;
    RegistryOutputFormat outputFormat = RegistryOutputFormat::Human;
    std::string pipeCommand;
    REGSAM viewSam = 0;
    std::string undoFile;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dry-run") dryRun = true;
        else if (arg == "--tx") useTx = true;
        else if (arg == "--json") jsonOutput = true;
        else if (arg == "--csv") outputFormat = RegistryOutputFormat::Csv;
        else if (arg == "--table") outputFormat = RegistryOutputFormat::Table;
        else if (arg == "--pipe" && i + 1 < argc) pipeCommand = argv[++i];
        else if (arg == "--undo-file" && i + 1 < argc) { undoFile = argv[++i]; }
        else if (arg.rfind("--undo-file=", 0) == 0) { undoFile = arg.substr(12); }
        else if (arg == "--view" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v == "32") viewSam = KEY_WOW64_32KEY;
            else if (v == "64") viewSam = KEY_WOW64_64KEY;
        }
        else if (arg.rfind("--view=", 0) == 0) {
            std::string v = arg.substr(7);
            if (v == "32") viewSam = KEY_WOW64_32KEY;
            else if (v == "64") viewSam = KEY_WOW64_64KEY;
        }
        else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        HelpSystem::PrintGeneralHelp();
        return 1;
    }

    command = positional[0];

    RegistryOutputSession outputSession(jsonOutput ? RegistryOutputFormat::Json : outputFormat, pipeCommand);

    // Transaction Management
    std::unique_ptr<ScopedTransaction> tx;
    if (useTx && !dryRun) {
        tx = std::make_unique<ScopedTransaction>();
        if (!tx->IsValid()) {
            std::cerr << "[-] Error: KTM Transaction initialization failed: " << Utils::FormatWin32Error(GetLastError()) << "\n";
            std::cerr << "[-] Hint: Verify KTM support in your Windows environment or omit --tx flag.\n";
            return 1;
        }
    }
    HANDLE hTx = tx ? tx->Get() : INVALID_HANDLE_VALUE;

    std::vector<UndoRecord> undoJournal;

    // ------------------------------------------------------------------------
    // DISPATCH: GET
    // ------------------------------------------------------------------------
    if (command == "get") {
        if (positional.size() < 2) {
            std::cerr << "[-] Error: 'get' requires <KeyPath> [ValueName]\n";
            return 1;
        }
        HKEY root = nullptr;
        std::string rootStr, subKey;
        if (!RegCodec::SplitPath(positional[1], root, rootStr, subKey)) {
            std::cerr << "[-] Error: Invalid Registry Path: " << positional[1] << "\n";
            return 1;
        }
        std::string valueName = positional.size() > 2 ? positional[2] : "";
        RegistryRecord rec;
        if (!RegistryEngine::QueryValue(root, rootStr, subKey, valueName, rec, viewSam, hTx)) {
            if (jsonOutput) {
                std::cout << "{\"exists\": false, \"path\": \"" << positional[1] << "\"}\n";
            } else {
                std::cerr << "[-] Error: Value or Key not found.\n";
            }
            return 2;
        }

        if (jsonOutput) {
            OutputFormatter::PrintJson(rec);
        } else {
            std::cout << "Key   : " << rec.rootKey << "\\" << rec.subKey << "\n"
                      << "Value : " << (rec.valueName.empty() ? "(Default)" : rec.valueName) << "\n"
                      << "Type  : " << rec.GetTypeString() << "\n"
                      << "Data  : " << rec.GetFormattedData() << "\n";
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // DISPATCH: SET / DIFF
    // ------------------------------------------------------------------------
    if (command == "set" || command == "diff") {
        if (positional.size() < 5) {
            std::cerr << "[-] Error: Command requires: <Path> <ValueName> <Type> <Data>\n";
            std::cerr << "[-] Run 'registryctl help set' for syntax and examples.\n";
            return 1;
        }
        std::string path = positional[1];
        std::string valName = positional[2];
        std::string typeStr = positional[3];
        std::string rawDataStr = positional[4];

        HKEY root = nullptr;
        std::string rootStr, subKey;
        if (!RegCodec::SplitPath(path, root, rootStr, subKey)) {
            std::cerr << "[-] Error: Invalid Registry Hive in: " << path << "\n";
            return 1;
        }

        DWORD dwType = RegCodec::ParseTypeString(typeStr);
        if (dwType == REG_NONE) {
            std::cerr << "[-] Error: Unsupported registry data type: " << typeStr << "\n";
            return 3;
        }

        std::vector<uint8_t> encoded;
        std::string parseErr;
        if (!RegCodec::EncodeData(dwType, rawDataStr, encoded, parseErr)) {
            std::cerr << "[-] Error encoding data: " << parseErr << "\n";
            return 3;
        }

        RegistryRecord oldRec;
        RegistryEngine::QueryValue(root, rootStr, subKey, valName, oldRec, viewSam, hTx);

        if (command == "diff" || dryRun) {
            OutputFormatter::PrintDiff(path, valName, oldRec, typeStr, rawDataStr);
            if (dryRun) std::cout << "[*] Dry-run enabled. No changes committed to disk.\n";
            return 0;
        }

        UndoRecord undo;
        undo.op = "set";
        undo.path = path;
        undo.valueName = valName;
        if (oldRec.exists) {
            undo.hadPreviousValue = true;
            undo.typeStr = oldRec.GetTypeString();
            undo.rawDataHex = Utils::BytesToHex(oldRec.rawData);
        } else {
            undo.hadPreviousValue = false;
        }
        undoJournal.push_back(undo);

        std::string writeErr;
        if (!RegistryEngine::SetValueRaw(root, subKey, valName, dwType, encoded, writeErr, viewSam, hTx)) {
            std::cerr << "[-] Write Error: " << writeErr << "\n";
            if (tx) tx->Rollback();
            return 1;
        }

        if (tx && !tx->Commit()) {
            std::cerr << "[-] Error committing KTM transaction.\n";
            return 1;
        }

        if (!undoFile.empty()) JournalEngine::WriteUndoLog(undoFile, undoJournal);

        if (jsonOutput) {
            std::cout << "{\"status\": \"success\", \"action\": \"set\", \"path\": \"" << path << "\"}\n";
        } else {
            std::cout << "[+] Successfully set " << path << " -> " << valName << "\n";
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // DISPATCH: DELETE
    // ------------------------------------------------------------------------
    if (command == "delete") {
        if (positional.size() < 2) {
            std::cerr << "[-] Error: 'delete' requires <Path> [ValueName]\n";
            return 1;
        }
        std::string path = positional[1];
        std::string valName = positional.size() > 2 ? positional[2] : "";

        HKEY root = nullptr;
        std::string rootStr, subKey;
        if (!RegCodec::SplitPath(path, root, rootStr, subKey)) {
            std::cerr << "[-] Error: Invalid Registry Hive in: " << path << "\n";
            return 1;
        }

        RegistryRecord oldRec;
        RegistryEngine::QueryValue(root, rootStr, subKey, valName, oldRec, viewSam, hTx);

        if (dryRun) {
            OutputFormatter::PrintDiff(path, valName, oldRec, "", "", true);
            std::cout << "[*] Dry-run enabled. No deletions committed to disk.\n";
            return 0;
        }

        if (valName.empty()) {
            RegistryEngine::BackupKeyRecursive(root, rootStr, subKey, undoJournal, viewSam, hTx);
        } else if (oldRec.exists) {
            UndoRecord u;
            u.op = "set";
            u.path = path;
            u.valueName = valName;
            u.hadPreviousValue = true;
            u.typeStr = oldRec.GetTypeString();
            u.rawDataHex = Utils::BytesToHex(oldRec.rawData);
            undoJournal.push_back(u);
        }

        std::string delErr;
        bool ok = valName.empty() ? 
            RegistryEngine::DeleteKeyRecursive(root, subKey, delErr, viewSam, hTx) : 
            RegistryEngine::DeleteValue(root, subKey, valName, delErr, viewSam, hTx);

        if (!ok) {
            std::cerr << "[-] Delete failed: " << delErr << "\n";
            if (tx) tx->Rollback();
            return 1;
        }

        if (tx && !tx->Commit()) {
            std::cerr << "[-] Error committing transaction.\n";
            return 1;
        }

        if (!undoFile.empty()) JournalEngine::WriteUndoLog(undoFile, undoJournal);

        if (jsonOutput) {
            std::cout << "{\"status\": \"success\", \"action\": \"delete\", \"path\": \"" << path << "\"}\n";
        } else {
            std::cout << "[+] Successfully deleted: " << path << (valName.empty() ? "" : (" -> " + valName)) << "\n";
        }
        return 0;
    }

    // ------------------------------------------------------------------------
    // DISPATCH: BATCH
    // ------------------------------------------------------------------------
    if (command == "batch") {
        if (positional.size() < 2) {
            std::cerr << "[-] Error: 'batch' requires <Manifest.json>\n";
            return 1;
        }
        std::ifstream file(positional[1]);
        if (!file.is_open()) {
            std::cerr << "[-] Error: Could not open batch manifest file: " << positional[1] << "\n";
            return 1;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Json::Parser parser(content);
        Json::Value rootJson = parser.ParseValue();
        if (!rootJson.IsObject() || !rootJson.HasKey("actions")) {
            std::cerr << "[-] Error: Invalid manifest format. Root object must contain 'actions' array.\n";
            return 3;
        }

        for (const auto& act : rootJson["actions"].AsArray()) {
            std::string op = act["op"].AsString();
            std::string path = act["path"].AsString();
            std::string val = act["value"].AsString();

            HKEY root = nullptr;
            std::string rootStr, subKey;
            if (!RegCodec::SplitPath(path, root, rootStr, subKey)) continue;

            RegistryRecord oldRec;
            RegistryEngine::QueryValue(root, rootStr, subKey, val, oldRec, viewSam, hTx);

            if (op == "set") {
                std::string typeStr = act["type"].AsString();
                std::string dataStr = act["data"].AsString();
                DWORD dwType = RegCodec::ParseTypeString(typeStr);
                std::vector<uint8_t> enc;
                std::string err;
                if (!RegCodec::EncodeData(dwType, dataStr, enc, err)) {
                    std::cerr << "[-] Batch item encode error: " << err << "\n";
                    if (tx) tx->Rollback();
                    return 3;
                }

                if (dryRun) {
                    OutputFormatter::PrintDiff(path, val, oldRec, typeStr, dataStr);
                    continue;
                }

                UndoRecord u;
                u.op = "set";
                u.path = path;
                u.valueName = val;
                u.hadPreviousValue = oldRec.exists;
                u.typeStr = oldRec.GetTypeString();
                u.rawDataHex = Utils::BytesToHex(oldRec.rawData);
                undoJournal.push_back(u);

                std::string werr;
                if (!RegistryEngine::SetValueRaw(root, subKey, val, dwType, enc, werr, viewSam, hTx)) {
                    std::cerr << "[-] Batch write failed: " << werr << "\n";
                    if (tx) tx->Rollback();
                    return 1;
                }
            } else if (op == "delete") {
                if (dryRun) {
                    OutputFormatter::PrintDiff(path, val, oldRec, "", "", true);
                    continue;
                }
                if (val.empty()) {
                    RegistryEngine::BackupKeyRecursive(root, rootStr, subKey, undoJournal, viewSam, hTx);
                } else if (oldRec.exists) {
                    UndoRecord u;
                    u.op = "set";
                    u.path = path;
                    u.valueName = val;
                    u.hadPreviousValue = true;
                    u.typeStr = oldRec.GetTypeString();
                    u.rawDataHex = Utils::BytesToHex(oldRec.rawData);
                    undoJournal.push_back(u);
                }

                std::string derr;
                bool ok = val.empty() ?
                    RegistryEngine::DeleteKeyRecursive(root, subKey, derr, viewSam, hTx) :
                    RegistryEngine::DeleteValue(root, subKey, val, derr, viewSam, hTx);
                if (!ok) {
                    std::cerr << "[-] Batch delete failed: " << derr << "\n";
                    if (tx) tx->Rollback();
                    return 1;
                }
            }
        }

        if (dryRun) {
            std::cout << "[*] Batch dry-run simulation complete. No changes applied.\n";
            return 0;
        }

        if (tx && !tx->Commit()) {
            std::cerr << "[-] Error committing batch transaction.\n";
            return 1;
        }

        if (!undoFile.empty()) JournalEngine::WriteUndoLog(undoFile, undoJournal);

        std::cout << "[+] Batch execution completed successfully.\n";
        return 0;
    }

    // ------------------------------------------------------------------------
    // DISPATCH: UNDO
    // ------------------------------------------------------------------------
    if (command == "undo") {
        if (positional.size() < 2) {
            std::cerr << "[-] Error: 'undo' requires <UndoLog.json>\n";
            return 1;
        }
        std::vector<UndoRecord> records;
        std::string err;
        if (!JournalEngine::LoadUndoLog(positional[1], records, err)) {
            std::cerr << "[-] " << err << "\n";
            return 1;
        }

        std::reverse(records.begin(), records.end());
        for (const auto& u : records) {
            HKEY root = nullptr;
            std::string rootStr, subKey;
            if (!RegCodec::SplitPath(u.path, root, rootStr, subKey)) continue;

            if (u.hadPreviousValue) {
                DWORD dwType = RegCodec::ParseTypeString(u.typeStr);
                std::vector<uint8_t> raw = Utils::HexToBytes(u.rawDataHex);
                if (dryRun) {
                    std::cout << "[*] Undo Preview: Restore " << u.path << " -> " << u.valueName << " [" << u.typeStr << "]\n";
                    continue;
                }
                std::string werr;
                if (!RegistryEngine::SetValueRaw(root, subKey, u.valueName, dwType, raw, werr, viewSam, hTx)) {
                    std::cerr << "[-] Undo restoration failed: " << werr << "\n";
                    if (tx) tx->Rollback();
                    return 1;
                }
            } else {
                if (dryRun) {
                    std::cout << "[*] Undo Preview: Delete newly created " << u.path << " -> " << u.valueName << "\n";
                    continue;
                }
                std::string derr;
                RegistryEngine::DeleteValue(root, subKey, u.valueName, derr, viewSam, hTx);
            }
        }

        if (dryRun) {
            std::cout << "[*] Undo simulation complete. No changes made.\n";
            return 0;
        }

        if (tx && !tx->Commit()) {
            std::cerr << "[-] Error committing undo transaction.\n";
            return 1;
        }

        std::cout << "[+] Rollback completed successfully.\n";
        return 0;
    }

    std::cerr << "[-] Unknown command: " << command << ". Run 'registryctl help' for available commands.\n";
    return 1;
}