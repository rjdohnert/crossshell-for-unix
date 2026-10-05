/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <regex>
#include <memory>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <cerrno>
#include <cstdlib>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

namespace fs = std::filesystem;

namespace SedSafe {
    inline bool ParseInteger(const std::string& text, int& value) {
        if (text.empty()) return false;
        char* end = nullptr;
        errno = 0;
        long parsed = std::strtol(text.c_str(), &end, 10);
        if (errno != 0 || end == text.c_str() || *end != '\0') return false;
        if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
        return true;
    }

    inline bool CompileRegex(const std::string& pattern, std::regex& re, std::regex::flag_type flags = std::regex::ECMAScript) {
        if (pattern.size() > 4096) return false;
        try {
            re = std::regex(pattern, flags);
            return true;
        } catch (const std::regex_error&) {
            return false;
        }
    }
}

// ============================================================================
// 1. STRING UTILS & WINDOWS OBJECT LOADER
// ============================================================================

class SedStringUtils {
public:
    static std::string TranslateReplacement(const std::string& rep) {
        std::string out;
        for (size_t i = 0; i < rep.size(); ++i) {
            if (rep[i] == '&') {
                out += "$&";
            } else if (rep[i] == '\\' && i + 1 < rep.size()) {
                char next = rep[i + 1];
                if (next >= '1' && next <= '9') {
                    out += '$';
                    out += next;
                    ++i;
                } else if (next == '&') {
                    out += '&';
                    ++i;
                } else if (next == '\\') {
                    out += "\\\\";
                    ++i;
                } else {
                    out += '\\';
                    out += next;
                    ++i;
                }
            } else if (rep[i] == '$') {
                out += "$$";
            } else {
                out += rep[i];
            }
        }
        return out;
    }

    static std::string JsonEscape(const std::string& value) {
        std::string out;
        for (unsigned char ch : value) {
            if (ch == '"') out += "\\\"";
            else if (ch == '\\') out += "\\\\";
            else if (ch == '\b') out += "\\b";
            else if (ch == '\f') out += "\\f";
            else if (ch == '\n') out += "\\n";
            else if (ch == '\r') out += "\\r";
            else if (ch == '\t') out += "\\t";
            else if (ch < 0x20) out += ' ';
            else out += static_cast<char>(ch);
        }
        return out;
    }

#ifdef _WIN32
    static std::string WideUtf8(const wchar_t* value) {
        if (!value || *value == L'\0') return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1) return {};
        std::string out(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), size, nullptr, nullptr);
        return out;
    }

    static std::wstring Utf8ToWide(const std::string& value) {
        if (value.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (size <= 1) return {};
        std::wstring out(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
        return out;
    }

    static std::string VariantJson(const VARIANT& value) {
        VARIANT converted;
        VariantInit(&converted);
        if (FAILED(VariantChangeType(&converted, const_cast<VARIANT*>(&value), 0, VT_BSTR))) return "null";
        std::string out = "\"" + JsonEscape(WideUtf8(converted.bstrVal)) + "\"";
        VariantClear(&converted);
        return out;
    }
#endif
};

class SedWindowsObjectLoader {
public:
#ifdef _WIN32
    static bool LoadRegistry(const std::string& spec, std::string& output, std::string& error) {
        size_t slash = spec.find('\\');
        std::string rootName = slash == std::string::npos ? spec : spec.substr(0, slash);
        std::string subKey = slash == std::string::npos ? "" : spec.substr(slash + 1);
        HKEY root = nullptr;
        if (_stricmp(rootName.c_str(), "HKLM") == 0 || _stricmp(rootName.c_str(), "HKEY_LOCAL_MACHINE") == 0) root = HKEY_LOCAL_MACHINE;
        else if (_stricmp(rootName.c_str(), "HKCU") == 0 || _stricmp(rootName.c_str(), "HKEY_CURRENT_USER") == 0) root = HKEY_CURRENT_USER;
        else if (_stricmp(rootName.c_str(), "HKCR") == 0 || _stricmp(rootName.c_str(), "HKEY_CLASSES_ROOT") == 0) root = HKEY_CLASSES_ROOT;
        else if (_stricmp(rootName.c_str(), "HKU") == 0 || _stricmp(rootName.c_str(), "HKEY_USERS") == 0) root = HKEY_USERS;
        else { error = "invalid registry root '" + rootName + "'"; return false; }

        HKEY key = nullptr;
        std::wstring wideSubKey = SedStringUtils::Utf8ToWide(subKey);
        if (RegOpenKeyExW(root, wideSubKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
            error = "cannot open registry key '" + spec + "'";
            return false;
        }

        for (DWORD index = 0;; ++index) {
            wchar_t name[16384] = {};
            DWORD nameSize = sizeof(name) / sizeof(wchar_t);
            DWORD type = 0;
            DWORD dataSize = 65536;
            BYTE data[65536] = {};
            LONG status = RegEnumValueW(key, index, name, &nameSize, nullptr, &type, data, &dataSize);
            if (status == ERROR_NO_MORE_ITEMS) break;
            if (status != ERROR_SUCCESS) continue;

            std::string nameStr = SedStringUtils::WideUtf8(name);
            std::string value;
            if (type == REG_DWORD && dataSize >= sizeof(DWORD)) {
                value = std::to_string(*reinterpret_cast<DWORD*>(data));
            } else if (type == REG_QWORD && dataSize >= sizeof(ULONGLONG)) {
                value = std::to_string(*reinterpret_cast<ULONGLONG*>(data));
            } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
                value = SedStringUtils::WideUtf8(reinterpret_cast<wchar_t*>(data));
            } else if (type == REG_MULTI_SZ) {
                const wchar_t* p = reinterpret_cast<const wchar_t*>(data);
                while (*p) {
                    if (!value.empty()) value += "\\n";
                    value += SedStringUtils::WideUtf8(p);
                    p += wcslen(p) + 1;
                }
            } else {
                value.assign(reinterpret_cast<char*>(data), dataSize);
            }
            output += "{\"name\":\"" + SedStringUtils::JsonEscape(nameStr) + "\",\"value\":\"" + SedStringUtils::JsonEscape(value) + "\"}\n";
        }
        RegCloseKey(key);
        return true;
    }

    static bool LoadWmi(const std::string& spec, std::string& output, std::string& error) {
        size_t pipe = spec.find('|');
        std::string query = pipe == std::string::npos ? spec : spec.substr(0, pipe);
        std::wstring ns = L"ROOT\\CIMV2";
        if (pipe != std::string::npos) {
            ns = SedStringUtils::Utf8ToWide(spec.substr(pipe + 1));
            if (ns.empty()) ns = L"ROOT\\CIMV2";
        }
        HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool uninit = SUCCEEDED(init);
        if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
            error = "cannot initialize COM for WMI";
            return false;
        }
        IWbemLocator* locator = nullptr;
        IWbemServices* services = nullptr;
        IEnumWbemClassObject* rows = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
        if (SUCCEEDED(hr)) hr = locator->ConnectServer(_bstr_t(ns.c_str()), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        if (SUCCEEDED(hr)) hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        std::wstring wideQuery = SedStringUtils::Utf8ToWide(query);
        if (SUCCEEDED(hr)) hr = services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wideQuery.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &rows);
        if (SUCCEEDED(hr)) {
            IWbemClassObject* row = nullptr;
            ULONG count = 0;
            while (rows->Next(WBEM_INFINITE, 1, &row, &count) == S_OK && count == 1) {
                SAFEARRAY* names = nullptr;
                row->GetNames(nullptr, WBEM_FLAG_NONSYSTEM_ONLY, nullptr, &names);
                output += "{";
                if (names) {
                    LONG lo = 0, hi = -1;
                    SafeArrayGetLBound(names, 1, &lo);
                    SafeArrayGetUBound(names, 1, &hi);
                    for (LONG i = lo; i <= hi; ++i) {
                        BSTR property = nullptr;
                        SafeArrayGetElement(names, &i, &property);
                        VARIANT value;
                        VariantInit(&value);
                        row->Get(property, 0, &value, nullptr, nullptr);
                        if (i > lo) output += ",";
                        output += "\"" + SedStringUtils::JsonEscape(SedStringUtils::WideUtf8(property)) + "\":" + SedStringUtils::VariantJson(value);
                        VariantClear(&value);
                        SysFreeString(property);
                    }
                    SafeArrayDestroy(names);
                }
                output += "}\n";
                row->Release();
            }
        }
        if (rows) rows->Release();
        if (services) services->Release();
        if (locator) locator->Release();
        if (uninit) CoUninitialize();
        if (FAILED(hr)) {
            error = "WMI query failed";
            return false;
        }
        return true;
    }
#endif

    static bool LoadObjectInput(const std::string& source, std::string& output, std::string& error) {
#ifdef _WIN32
        if (source.rfind("registry:", 0) == 0) return LoadRegistry(source.substr(9), output, error);
        return LoadWmi(source.substr(4), output, error);
#else
        error = "registry and WMI input require Windows"; return false;
#endif
    }
};

// ============================================================================
// 2. EMBEDDED JSON ENGINE (READ / WRITE / MUTATE / SERIALIZE)
// ============================================================================
struct Value;
using Object = std::map<std::string, Value>;
using Array  = std::vector<Value>;

struct Value {
    enum Type { NIL, BOOL, NUMBER, STRING, ARRAY, OBJECT } type = NIL;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    Value() : type(NIL), data(nullptr) {}
    Value(bool b) : type(BOOL), data(b) {}
    Value(double d) : type(NUMBER), data(d) {}
    Value(const std::string& s) : type(STRING), data(s) {}
    Value(const char* s) : type(STRING), data(std::string(s)) {}
    Value(Array a) : type(ARRAY), data(std::move(a)) {}
    Value(Object o) : type(OBJECT), data(std::move(o)) {}

    std::string to_string() const {
        if (type == NIL) return "";
        if (type == BOOL) return std::get<bool>(data) ? "true" : "false";
        if (type == NUMBER) {
            double d = std::get<double>(data);
            if (d == static_cast<long long>(d)) return std::to_string(static_cast<long long>(d));
            std::ostringstream ss;
            ss << std::setprecision(10) << d;
            return ss.str();
        }
        if (type == STRING) return std::get<std::string>(data);
        return serialize_json();
    }

    double to_number() const {
        if (type == NUMBER) return std::get<double>(data);
        if (type == BOOL) return std::get<bool>(data) ? 1.0 : 0.0;
        if (type == STRING) {
            try { return std::stod(std::get<std::string>(data)); } catch (...) { return 0.0; }
        }
        return 0.0;
    }

    bool to_bool() const {
        if (type == NIL) return false;
        if (type == BOOL) return std::get<bool>(data);
        if (type == NUMBER) return std::get<double>(data) != 0.0;
        if (type == STRING) return !std::get<std::string>(data).empty() && std::get<std::string>(data) != "0";
        if (type == ARRAY) return !std::get<Array>(data).empty();
        if (type == OBJECT) return !std::get<Object>(data).empty();
        return true;
    }

    std::string serialize_json() const {
        if (type == NIL) return "null";
        if (type == BOOL) return std::get<bool>(data) ? "true" : "false";
        if (type == NUMBER) {
            double d = std::get<double>(data);
            if (d == static_cast<long long>(d)) return std::to_string(static_cast<long long>(d));
            std::ostringstream ss;
            ss << std::setprecision(10) << d;
            return ss.str();
        }
        if (type == STRING) {
            std::string s = std::get<std::string>(data);
            std::string out = "\"";
            for (char c : s) {
                if (c == '"') out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\t') out += "\\t";
                else if (c == '\r') out += "\\r";
                else out += c;
            }
            return out + "\"";
        }
        if (type == ARRAY) {
            std::string out = "[";
            const auto& arr = std::get<Array>(data);
            for (size_t i = 0; i < arr.size(); ++i) {
                out += arr[i].serialize_json() + (i + 1 < arr.size() ? ", " : "");
            }
            return out + "]";
        }
        if (type == OBJECT) {
            std::string out = "{";
            const auto& obj = std::get<Object>(data);
            size_t i = 0;
            for (const auto& [k, v] : obj) {
                out += "\"" + k + "\": " + v.serialize_json() + (++i < obj.size() ? ", " : "");
            }
            return out + "}";
        }
        return "null";
    }

    Value* get_path_mut(const std::string& key) {
        if (type == OBJECT) {
            auto& obj = std::get<Object>(data);
            auto it = obj.find(key);
            if (it != obj.end()) return &(it->second);
        } else if (type == ARRAY) {
            try {
                size_t idx = std::stoul(key);
                auto& arr = std::get<Array>(data);
                if (idx < arr.size()) return &arr[idx];
            } catch (...) {}
        }
        return nullptr;
    }

    const Value* get_path(const std::string& key) const {
        if (type == OBJECT) {
            const auto& obj = std::get<Object>(data);
            auto it = obj.find(key);
            if (it != obj.end()) return &(it->second);
        } else if (type == ARRAY) {
            try {
                size_t idx = std::stoul(key);
                const auto& arr = std::get<Array>(data);
                if (idx < arr.size()) return &arr[idx];
            } catch (...) {}
        }
        return nullptr;
    }

    bool erase_path(const std::string& key) {
        if (type == OBJECT) {
            auto& obj = std::get<Object>(data);
            return obj.erase(key) > 0;
        } else if (type == ARRAY) {
            try {
                size_t idx = std::stoul(key);
                auto& arr = std::get<Array>(data);
                if (idx < arr.size()) {
                    arr.erase(arr.begin() + idx);
                    return true;
                }
            } catch (...) {}
        }
        return false;
    }

    Value* resolve_ptr(const std::string& path_str, bool create_missing = false) {
        if (path_str.empty()) return this;
        std::string path = (path_str.front() == '.') ? path_str.substr(1) : path_str;

        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == '[') path[i] = '.';
            if (path[i] == ']') path.erase(i--, 1);
        }

        std::istringstream ss(path);
        std::string segment;
        Value* curr = this;

        while (std::getline(ss, segment, '.')) {
            if (segment.empty()) continue;
            if (curr->type != OBJECT && curr->type != ARRAY) {
                if (!create_missing) return nullptr;
                curr->type = OBJECT;
                curr->data = Object();
            }

            if (curr->type == OBJECT) {
                auto& obj = std::get<Object>(curr->data);
                if (obj.find(segment) == obj.end()) {
                    if (!create_missing) return nullptr;
                    obj[segment] = Value();
                }
                curr = &obj[segment];
            } else if (curr->type == ARRAY) {
                try {
                    size_t idx = std::stoul(segment);
                    auto& arr = std::get<Array>(curr->data);
                    if (idx >= arr.size()) {
                        if (!create_missing) return nullptr;
                        arr.resize(idx + 1);
                    }
                    curr = &arr[idx];
                } catch (...) {
                    return nullptr;
                }
            }
        }
        return curr;
    }

    bool del_path(const std::string& path_str) {
        if (path_str.empty()) return false;
        std::string path = (path_str.front() == '.') ? path_str.substr(1) : path_str;

        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == '[') path[i] = '.';
            if (path[i] == ']') path.erase(i--, 1);
        }

        size_t last_dot = path.rfind('.');
        std::string parent_path = (last_dot != std::string::npos) ? path.substr(0, last_dot) : "";
        std::string target_key  = (last_dot != std::string::npos) ? path.substr(last_dot + 1) : path;

        Value* parent = resolve_ptr(parent_path, false);
        if (!parent) return false;

        if (parent->type == OBJECT) {
            auto& obj = std::get<Object>(parent->data);
            return obj.erase(target_key) > 0;
        } else if (parent->type == ARRAY) {
            try {
                size_t idx = std::stoul(target_key);
                auto& arr = std::get<Array>(parent->data);
                if (idx < arr.size()) {
                    arr.erase(arr.begin() + idx);
                    return true;
                }
            } catch (...) {}
        }
        return false;
    }
};

class MiniJsonParser {
    std::string src;
    size_t pos = 0;

    void skip_ws() {
        while (pos < src.size() && (std::isspace(static_cast<unsigned char>(src[pos])))) pos++;
    }
    char peek() { skip_ws(); return pos < src.size() ? src[pos] : '\0'; }
    char get()  { skip_ws(); return pos < src.size() ? src[pos++] : '\0'; }

    std::string parse_string() {
        get();
        std::string s;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < src.size()) {
                char esc = src[pos++];
                if (esc == 'n') s += '\n';
                else if (esc == 't') s += '\t';
                else if (esc == 'r') s += '\r';
                else s += esc;
            } else {
                s += c;
            }
        }
        return s;
    }

    Value parse_number() {
        size_t start = pos;
        if (src[pos] == '-') pos++;
        while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
            pos++;
        }
        try {
            return Value(std::stod(src.substr(start, pos - start)));
        } catch (...) {
            return Value(0.0);
        }
    }

public:
    explicit MiniJsonParser(std::string s) : src(std::move(s)) {}

    Value parse_value() {
        char c = peek();
        if (c == '{') {
            get();
            Object obj;
            while (peek() != '}' && peek() != '\0') {
                if (peek() == ',') { get(); continue; }
                std::string k = parse_string();
                if (peek() == ':') get();
                obj[k] = parse_value();
            }
            if (peek() == '}') get();
            return Value(obj);
        } else if (c == '[') {
            get();
            Array arr;
            while (peek() != ']' && peek() != '\0') {
                if (peek() == ',') { get(); continue; }
                arr.push_back(parse_value());
            }
            if (peek() == ']') get();
            return Value(arr);
        } else if (c == '"') {
            return Value(parse_string());
        } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
            return parse_number();
        } else if (c == 't' || c == 'f') {
            std::string b;
            while (std::isalpha(static_cast<unsigned char>(peek()))) b += get();
            return Value(b == "true");
        } else if (c == 'n') {
            for (int i = 0; i < 4; ++i) get();
            return Value();
        }
        return Value();
    }
};

Value parse_scalar_literal(const std::string& raw_val) {
    std::string val_str = raw_val;
    while (val_str.size() >= 2 && ((val_str.front() == '"' && val_str.back() == '"') || (val_str.front() == '\'' && val_str.back() == '\''))) {
        val_str = val_str.substr(1, val_str.size() - 2);
    }
    while (val_str.rfind("\\\"", 0) == 0 && val_str.size() >= 4 && val_str.substr(val_str.size() - 2) == "\\\"") {
        val_str = val_str.substr(2, val_str.size() - 4);
    }
    if (val_str == "true") return Value(true);
    if (val_str == "false") return Value(false);
    if (val_str == "null") return Value();
    try {
        size_t p;
        double d = std::stod(val_str, &p);
        if (p == val_str.size()) return Value(d);
    } catch (...) {}
    return Value(val_str);
}

// ============================================================================
// 2. SED COMMAND DATA STRUCTURES & PARSER
// ============================================================================
enum class CommandType {
    SUBSTITUTE,     // s/re/rep/flags or s/.path/re/rep/flags
    SET_PROPERTY,   // set .path = value
    DEL_PROPERTY,   // del .path
    DELETE_LINE,    // d
    PRINT_LINE,     // p
    QUIT            // q
};

struct Address {
    enum Type { ALL, SINGLE_LINE, LINE_RANGE, REGEX_MATCH, PREDICATE } type = ALL;
    long long line1 = 0;
    long long line2 = 0;
    std::string regex_str = "";
    std::string predicate_str = "";
};

struct SedCommand {
    Address addr;
    CommandType type = CommandType::PRINT_LINE;
    std::string path = "";           // Object path for .key operations
    std::string find_regex = "";     // Regex to match
    std::string replacement = "";    // Replacement string
    Value set_val;                   // Literal value for `set`
    bool flag_global = false;        // 'g' flag
    bool flag_ignore_case = false;   // 'i' flag
    bool flag_print = false;         // 'p' flag
};

struct SedOptions {
    bool quiet = false;              // -n / --quiet
    bool in_place = false;           // -i / --in-place
    bool extended_regex = false;     // -E / -r
    bool separate_files = false;     // -s
    bool unbuffered = false;         // -u
    bool binary_mode = false;        // -b
    bool posix_mode = false;         // --posix
    bool sandbox_mode = false;       // --sandbox
    bool debug_mode = false;         // --debug
    bool follow_symlinks = false;    // --follow-symlinks
    bool end_of_options = false;     // --
    std::string backup_suffix = "";  // Backup suffix (e.g. .bak)
    bool json_only = false;          // -j
    bool text_only = false;          // -t
    char record_delim = '\n';        // -0 / --null-data
    std::vector<std::string> scripts;
    std::vector<std::string> files;
    std::vector<std::string> object_sources;
};

// ============================================================================
// 3. SED SCRIPT PARSER
// ============================================================================
class ScriptParser {
    static std::string trim(const std::string& s) {
        auto start = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
        auto end = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();
        return (end <= start ? std::string() : std::string(start, end));
    }

    static std::vector<std::string> split_statements(const std::string& script_text) {
        std::vector<std::string> stmts;
        std::string cur;
        bool in_sq = false;
        bool in_dq = false;
        char delim = '\0';
        int delim_needed = 0;

        for (size_t i = 0; i < script_text.size(); ++i) {
            char c = script_text[i];
            if (c == '\\' && i + 1 < script_text.size()) {
                cur += c;
                cur += script_text[++i];
                continue;
            }
            if (c == '\'' && !in_dq && delim_needed == 0) {
                in_sq = !in_sq;
                cur += c;
            } else if (c == '"' && !in_sq && delim_needed == 0) {
                in_dq = !in_dq;
                cur += c;
            } else if (!in_sq && !in_dq) {
                if (delim_needed > 0) {
                    if (c == delim) {
                        delim_needed--;
                    }
                    cur += c;
                } else {
                    if (c == ';' || c == '\n') {
                        std::string t = trim(cur);
                        if (!t.empty()) stmts.push_back(t);
                        cur.clear();
                    } else {
                        if (c == '/' && cur.empty()) {
                            delim = '/';
                            delim_needed = 1;
                        } else if (c == 's' && (cur.empty() || cur.back() == ':' || cur.back() == ' ' || std::isdigit(static_cast<unsigned char>(cur.back())) || cur.back() == ',')) {
                            if (i + 1 < script_text.size()) {
                                char next = script_text[i + 1];
                                if (!std::isalnum(static_cast<unsigned char>(next)) && !std::isspace(static_cast<unsigned char>(next)) && next != ';') {
                                    delim = next;
                                    delim_needed = 3;
                                }
                            }
                        }
                        cur += c;
                    }
                }
            } else {
                cur += c;
            }
        }
        std::string t = trim(cur);
        if (!t.empty()) stmts.push_back(t);
        return stmts;
    }

public:
    [[nodiscard]] std::vector<SedCommand> parse(const std::string& script_text) const {
        std::vector<SedCommand> commands;
        std::vector<std::string> raw_stmts = split_statements(script_text);

        for (const auto& raw_stmt : raw_stmts) {
            std::string stmt = trim(raw_stmt);
            if (stmt.empty() || stmt.front() == '#') continue;

            SedCommand cmd;
            size_t idx = 0;

            // 1. Parse Address / Predicate Prefix
            if (stmt.front() == '/') {
                size_t close_slash = std::string::npos;
                bool esc = false;
                for (size_t k = 1; k < stmt.size(); ++k) {
                    if (esc) {
                        esc = false;
                    } else if (stmt[k] == '\\') {
                        esc = true;
                    } else if (stmt[k] == '/') {
                        close_slash = k;
                        break;
                    }
                }
                if (close_slash != std::string::npos) {
                    cmd.addr.type = Address::REGEX_MATCH;
                    cmd.addr.regex_str = stmt.substr(1, close_slash - 1);
                    idx = close_slash + 1;
                }
            } else if (std::isdigit(static_cast<unsigned char>(stmt.front()))) {
                size_t p = 0;
                cmd.addr.line1 = std::stoll(stmt, &p);
                idx = p;
                if (idx < stmt.size() && stmt[idx] == ',') {
                    idx++;
                    size_t p2 = 0;
                    cmd.addr.line2 = std::stoll(stmt.substr(idx), &p2);
                    idx += p2;
                    cmd.addr.type = Address::LINE_RANGE;
                } else {
                    cmd.addr.type = Address::SINGLE_LINE;
                }
            } else if (stmt.find(':') != std::string::npos && (!stmt.empty() && (stmt.front() == '.' || stmt.find("==") != std::string::npos || stmt.find(">=") != std::string::npos || stmt.find("<=") != std::string::npos))) {
                size_t colon = stmt.find(':');
                cmd.addr.type = Address::PREDICATE;
                cmd.addr.predicate_str = trim(stmt.substr(0, colon));
                idx = colon + 1;
            }

            while (idx < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[idx]))) idx++;
            if (idx >= stmt.size()) continue;

            std::string op_part = stmt.substr(idx);

            // 2. Parse Commands
            if (!op_part.empty() && op_part.front() == 'd') {
                cmd.type = CommandType::DELETE_LINE;
            } else if (!op_part.empty() && op_part.front() == 'p') {
                cmd.type = CommandType::PRINT_LINE;
            } else if (!op_part.empty() && op_part.front() == 'q') {
                cmd.type = CommandType::QUIT;
            } else if (op_part.rfind("del ", 0) == 0 || op_part.rfind("unset ", 0) == 0) {
                cmd.type = CommandType::DEL_PROPERTY;
                size_t sp = op_part.find(' ');
                cmd.path = trim(op_part.substr(sp + 1));
            } else if (op_part.rfind("set ", 0) == 0) {
                cmd.type = CommandType::SET_PROPERTY;
                size_t eq = op_part.find('=');
                if (eq != std::string::npos) {
                    cmd.path = trim(op_part.substr(4, eq - 4));
                    cmd.set_val = parse_scalar_literal(trim(op_part.substr(eq + 1)));
                }
            } else if (!op_part.empty() && op_part.front() == 's') {
                cmd.type = CommandType::SUBSTITUTE;
                char delim = op_part.size() > 1 ? op_part[1] : '/';
                std::vector<std::string> tokens;
                std::string current;
                size_t i = 2;

                while (i < op_part.size()) {
                    char c = op_part[i];
                    if (c == '\\' && i + 1 < op_part.size()) {
                        char next = op_part[i + 1];
                        if (next == delim) {
                            current += delim;
                            i += 2;
                        } else if (next == '\\') {
                            current += '\\';
                            i += 2;
                        } else {
                            current += '\\';
                            current += next;
                            i += 2;
                        }
                    } else if (c == delim) {
                        tokens.push_back(current);
                        current.clear();
                        i++;
                        if ((!tokens.empty() && !tokens[0].empty() && tokens[0].front() == '.' && tokens.size() == 3) ||
                            ((tokens.empty() || tokens[0].empty() || tokens[0].front() != '.') && tokens.size() == 2)) {
                            std::string flags_str = op_part.substr(i);
                            tokens.push_back(flags_str);
                            current.clear();
                            break;
                        }
                    } else {
                        current += c;
                        i++;
                    }
                }
                if (!current.empty() || tokens.size() < 2) {
                    tokens.push_back(current);
                }

                std::string flags_token = "";
                if (!tokens.empty() && !tokens[0].empty() && tokens[0].front() == '.') {
                    cmd.path = tokens[0];
                    if (tokens.size() > 1) cmd.find_regex = tokens[1];
                    if (tokens.size() > 2) cmd.replacement = tokens[2];
                    if (tokens.size() > 3) flags_token = tokens[3];
                } else {
                    if (tokens.size() > 0) cmd.find_regex = tokens[0];
                    if (tokens.size() > 1) cmd.replacement = tokens[1];
                    if (tokens.size() > 2) flags_token = tokens[2];
                }

                for (char f : flags_token) {
                    if (f == 'g') cmd.flag_global = true;
                    if (f == 'i') cmd.flag_ignore_case = true;
                    if (f == 'p') cmd.flag_print = true;
                }
            }

            commands.push_back(cmd);
        }
        return commands;
    }
};

// ============================================================================
// 4. PREDICATE & ADDRESS EVALUATOR
// ============================================================================
class Evaluator {
    static std::string trim(const std::string& s) {
        auto start = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
        auto end = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();
        return (end <= start ? std::string() : std::string(start, end));
    }

public:
    [[nodiscard]] bool eval_condition(const std::string& expr, const Value& obj) const {
        if (expr.empty()) return true;

        size_t or_pos = expr.find("||");
        if (or_pos != std::string::npos) {
            return eval_condition(expr.substr(0, or_pos), obj) || eval_condition(expr.substr(or_pos + 2), obj);
        }

        size_t and_pos = expr.find("&&");
        if (and_pos != std::string::npos) {
            return eval_condition(expr.substr(0, and_pos), obj) && eval_condition(expr.substr(and_pos + 2), obj);
        }

        std::string trimmed = trim(expr);
        if (trimmed.empty()) return true;

        if (trimmed.front() == '!') {
            return !eval_condition(trimmed.substr(1), obj);
        }

        try {
            std::regex op_re;
            std::smatch match;
            if (SedSafe::CompileRegex(R"((.+?)\s*(==|!=|>=|<=|>|<|~|!~)\s*(.+))", op_re) && std::regex_match(trimmed, match, op_re)) {
                std::string lhs_str = trim(match[1].str());
                std::string op      = match[2].str();
                std::string rhs_str = trim(match[3].str());

                Value lhs = (!lhs_str.empty() && lhs_str.front() == '.') ?
                    (((Value&)obj).resolve_ptr(lhs_str) ? *((Value&)obj).resolve_ptr(lhs_str) : Value()) :
                    parse_scalar_literal(lhs_str);
                Value rhs = (!rhs_str.empty() && rhs_str.front() == '.') ?
                    (((Value&)obj).resolve_ptr(rhs_str) ? *((Value&)obj).resolve_ptr(rhs_str) : Value()) :
                    parse_scalar_literal(rhs_str);

                if (op == "==") return lhs.to_string() == rhs.to_string();
                if (op == "!=") return lhs.to_string() != rhs.to_string();
                if (op == "~" || op == "!~") {
                    std::regex re;
                    if (!SedSafe::CompileRegex(rhs.to_string(), re)) return false;
                    bool matched = std::regex_search(lhs.to_string(), re);
                    return (op == "~") ? matched : !matched;
                }

                double l_num = lhs.to_number();
                double r_num = rhs.to_number();
                if (op == "<")  return l_num < r_num;
                if (op == "<=") return l_num <= r_num;
                if (op == ">")  return l_num > r_num;
                if (op == ">=") return l_num >= r_num;
            }
        } catch (const std::regex_error&) {
            return false;
        }

        if (!trimmed.empty() && trimmed.front() == '.') {
            Value* v = ((Value&)obj).resolve_ptr(trimmed);
            return v ? v->to_bool() : false;
        }

        return !trimmed.empty();
    }

    [[nodiscard]] bool match_address(const Address& addr, long long line_nr, const std::string& line, const Value& obj) const {
        switch (addr.type) {
            case Address::ALL: return true;
            case Address::SINGLE_LINE: return line_nr == addr.line1;
            case Address::LINE_RANGE:  return line_nr >= addr.line1 && line_nr <= addr.line2;
            case Address::REGEX_MATCH: {
                std::regex re;
                if (!SedSafe::CompileRegex(addr.regex_str, re)) return false;
                return std::regex_search(line, re);
            }
            case Address::PREDICATE: return eval_condition(addr.predicate_str, obj);
        }
        return true;
    }
};

// ============================================================================
// 5. STREAM & OBJECT TRANSFORMATION ENGINE
// ============================================================================
class SedEngine {
public:
    SedEngine(const std::vector<SedCommand>& commands, const SedOptions& options)
        : m_commands(commands), m_options(options) {}

    void ProcessStream(std::istream& in, std::ostream& out) const {
        std::string line;
        long long line_nr = 0;
        const char output_delim = m_options.record_delim;

        while (std::getline(in, line, m_options.record_delim)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            line_nr++;

            Value parsed_obj;
            bool is_json = false;

            if (!m_options.text_only) {
                std::string trimmed = line;
                trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
                    MiniJsonParser parser(trimmed);
                    parsed_obj = parser.parse_value();
                    is_json = (parsed_obj.type == Value::OBJECT || parsed_obj.type == Value::ARRAY);
                }
            }

            if (m_options.json_only && !is_json) continue;

            bool deleted = false;
            bool explicitly_printed = false;
            bool should_quit = false;

            for (const auto& cmd : m_commands) {
                if (!m_evaluator.match_address(cmd.addr, line_nr, line, parsed_obj)) {
                    continue;
                }

                switch (cmd.type) {
                    case CommandType::DELETE_LINE:
                        deleted = true;
                        break;

                    case CommandType::PRINT_LINE:
                        out << (is_json ? parsed_obj.serialize_json() : line) << output_delim;
                        explicitly_printed = true;
                        break;

                    case CommandType::QUIT:
                        should_quit = true;
                        break;

                    case CommandType::SET_PROPERTY:
                        if (is_json) {
                            Value* target = parsed_obj.resolve_ptr(cmd.path, true);
                            if (target) *target = cmd.set_val;
                        }
                        break;

                    case CommandType::DEL_PROPERTY:
                        if (is_json) {
                            parsed_obj.del_path(cmd.path);
                        }
                        break;

                    case CommandType::SUBSTITUTE: {
                        try {
                            std::regex::flag_type flags = std::regex::ECMAScript;
                            if (cmd.flag_ignore_case) flags |= std::regex::icase;
                            std::regex re;
                            if (!SedSafe::CompileRegex(cmd.find_regex, re, flags)) {
                                std::cerr << "sed: invalid regular expression: " << cmd.find_regex << "\n";
                                break;
                            }
                            bool substituted = false;
                            std::string translated_rep = SedStringUtils::TranslateReplacement(cmd.replacement);

                            if (is_json && !cmd.path.empty()) {
                                Value* target = parsed_obj.resolve_ptr(cmd.path, false);
                                if (target && target->type == Value::STRING) {
                                    std::string val_str = std::get<std::string>(target->data);
                                    if (std::regex_search(val_str, re)) {
                                        std::string replaced = cmd.flag_global ? 
                                            std::regex_replace(val_str, re, translated_rep) :
                                            std::regex_replace(val_str, re, translated_rep, std::regex_constants::format_first_only);
                                        *target = Value(replaced);
                                        substituted = true;
                                    }
                                }
                            } else {
                                if (std::regex_search(line, re)) {
                                    line = cmd.flag_global ? 
                                        std::regex_replace(line, re, translated_rep) : 
                                        std::regex_replace(line, re, translated_rep, std::regex_constants::format_first_only);
                                    substituted = true;
                                }
                            }

                            if (cmd.flag_print && substituted) {
                                out << (is_json ? parsed_obj.serialize_json() : line) << output_delim;
                                explicitly_printed = true;
                            }
                        } catch (const std::regex_error& e) {
                            std::cerr << "sed: regex error in substitution: " << e.what() << "\n";
                        }
                        break;
                    }
                }

                if (deleted || should_quit) break;
            }

            if (!deleted && !m_options.quiet && !explicitly_printed) {
                out << (is_json ? parsed_obj.serialize_json() : line) << output_delim;
            }

            if (m_options.unbuffered) out.flush();

            if (should_quit) break;
        }
    }

private:
    const std::vector<SedCommand>& m_commands;
    const SedOptions& m_options;
    Evaluator m_evaluator;
};

// ============================================================================
// 6. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================
class OptionParser {
public:
    static void DisplayHelp() {
        std::cout <<
R"(sed(1)                CrossShell for UNIX Reference Manual                sed(1)

NAME
    sed - stream editor for filtering and transforming text

SYNOPSIS
    sed [OPTIONS] SCRIPT [FILE...]
    sed [OPTIONS] -e SCRIPT... [FILE...]
    <input-stream> | sed [OPTIONS] SCRIPT

DESCRIPTION
    sed is a stream editor used to perform basic text and structured object
    transformations on an input stream (a file or input from a pipeline).
    It natively understands both line-oriented text stream transformations
    and JSON/Object manipulations (nested mutation, key deletion, and field
    substitution).

OPTIONS
    -e, --expression=SCRIPT
        Add script commands to the execution pipeline. Multiple -e arguments
        can be chained together or separated by ';'.
    -f, --file=SCRIPT_FILE
        Read transformation commands directly from a script file.
    -i[SUFFIX], --in-place[=SUFFIX]
        Edit files in-place (overwriting original). If SUFFIX is supplied
        (e.g., -i.bak), creates a backup of the original file.
    -n, --quiet, --silent
        Suppress automatic printing of pattern space. Output is produced
        only when explicitly requested via the 'p' command or 'p' flag.
    -0, --null-data
        Separate records by NUL characters (\0) instead of newlines.
    -j, --json-only
        Force pure JSON mode. Non-JSON records are skipped.
    -t, --text-only
        Disable JSON object auto-detection. Treat all inputs as pure text.
    -E, -r, --regexp-extended
        Use extended regular expression syntax.
    -s, --separate
        Treat each input file as a separate stream for addressing.
    -u, --unbuffered
        Flush output after each processed record.
    -b, --binary
        Read and write input as binary data where supported.
    --posix
        Select POSIX compatibility behavior.
    --sandbox
        Select restricted execution behavior.
    --debug
        Enable diagnostic processing output.
    --follow-symlinks
        Follow symlinks when performing in-place edits.
    --registry KEY
        Read registry values as JSON Lines before applying the script.
    --wmi QUERY[|NAMESPACE]
        Read WMI objects as JSON Lines (default namespace ROOT\CIMV2).
    -h, --help
        Display this comprehensive reference manual and exit.
    -V, --version
        Display version and environment information and exit.

COMMAND SYNTAX
    Plain Text Substitution:
        s/regexp/replacement/[flags]
        s#regexp#replacement#[flags]
        Flags:
            g   Replace globally (all occurrences in line/field).
            i   Case-insensitive regular expression match.
            p   Print the pattern space if a substitution was made.

    Field-Scoped Object Substitution:
        s/.path.to.key/regexp/replacement/[flags]
        Performs regular expression substitution exclusively within the target
        nested JSON object or array field.

    Object Property Mutations:
        set .path.to.key = <value>
            Sets or adds a nested field in a JSON record.
        del .path.to.key
            Deletes a specific key from a JSON object or array item.

    Record Flow and Control:
        d   Delete line/record (prevents it from being printed).
        p   Print the current pattern space immediately.
        q   Quit sed immediately (stops reading further input).

ADDRESSING
    Commands can be prefixed with address conditions:
        3d                           Delete line 3.
        1,5s/foo/bar/g               Substitute 'foo' with 'bar' on lines 1 to 5.
        /ERROR/d                     Delete all lines matching regex 'ERROR'.
        .status >= 500:d             Delete JSON records where status >= 500.
        .env == "prod":set .debug=0  Set debug to 0 on records where env is 'prod'.

EXAMPLES
    sed "s/http:\/\//https:\/\//g" urls.txt
        Replace http with https across input text file.

    sed -i.bak "s/DEBUG/INFO/g" server.log
        In-place replace with backup file creation.

    sed ".user.id == 101:set .user.role = \"admin\"" users.jsonl
        Mutate nested JSON property conditionally.

    sed "s/.user.email/@.*$/@redacted.com/g" accounts.jsonl
        Redact email domain within JSON object stream.

    sed "del .meta.trace_id; del .meta.debug" telemetry.jsonl
        Delete metadata fields from JSON stream.

EXIT STATUS
    0   Success.
    1   Syntax error, unknown command, or execution failure.

    CrossShell for UNIX                                                   sed(1)
)";
    }

    static void DisplayVersion() {
        std::cout << "sed version 2.0.0\n"
                  << "Copyright (C) 2026, Roberto J Dohnert\n";
    }

    bool Parse(int argc, char* argv[], SedOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        std::string script_arg = "";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (opts.end_of_options) {
                if (opts.scripts.empty() && script_arg.empty()) script_arg = arg;
                else opts.files.push_back(arg);
                continue;
            }
            if (arg == "--") {
                opts.end_of_options = true;
                continue;
            }

            if (arg == "-h" || arg == "--help") { DisplayHelp(); exitEarly = true; return true; }
            if (arg == "-V" || arg == "--version") { DisplayVersion(); exitEarly = true; return true; }
            else if (arg == "-n" || arg == "--quiet" || arg == "--silent") opts.quiet = true;
            else if (arg == "-E" || arg == "-r" || arg == "--regexp-extended") opts.extended_regex = true;
            else if (arg == "-s" || arg == "--separate") opts.separate_files = true;
            else if (arg == "-u" || arg == "--unbuffered") opts.unbuffered = true;
            else if (arg == "-b" || arg == "--binary") opts.binary_mode = true;
            else if (arg == "--posix") opts.posix_mode = true;
            else if (arg == "--sandbox") opts.sandbox_mode = true;
            else if (arg == "--debug") opts.debug_mode = true;
            else if (arg == "--follow-symlinks") opts.follow_symlinks = true;
            else if (arg == "-j" || arg == "--json-only") opts.json_only = true;
            else if (arg == "-t" || arg == "--text-only") opts.text_only = true;
            else if (arg == "-0" || arg == "--null-data") opts.record_delim = '\0';
            else if (arg == "-i") {
                opts.in_place = true;
                if (i + 1 < argc && argv[i + 1][0] == '.') {
                    opts.backup_suffix = argv[++i];
                }
            } else if (arg.rfind("-i", 0) == 0) {
                opts.in_place = true;
                opts.backup_suffix = arg.substr(2);
            } else if (arg.rfind("--in-place", 0) == 0) {
                opts.in_place = true;
                if (arg.find('=') != std::string::npos) {
                    opts.backup_suffix = arg.substr(arg.find('=') + 1);
                } else if (i + 1 < argc && argv[i + 1][0] == '.') {
                    opts.backup_suffix = argv[++i];
                }
            } else if ((arg == "-e" || arg == "--expression") && i + 1 < argc) {
                opts.scripts.push_back(argv[++i]);
            } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
                std::ifstream sf(argv[++i]);
                if (!sf.is_open()) {
                    std::cerr << "sed: cannot read script file " << argv[i] << "\n";
                    return false;
                }
                std::string content((std::istreambuf_iterator<char>(sf)), std::istreambuf_iterator<char>());
                opts.scripts.push_back(content);
            } else if ((arg == "--registry" || arg == "--wmi") && i + 1 < argc) {
                opts.object_sources.push_back((arg == "--registry" ? "registry:" : "wmi:") + std::string(argv[++i]));
            } else if (!arg.empty() && arg[0] != '-') {
                if (opts.scripts.empty() && script_arg.empty()) {
                    script_arg = arg;
                    if (script_arg.rfind("set ", 0) == 0) {
                        while (i + 1 < argc && (script_arg.find('=') == std::string::npos || script_arg.back() == '=' || script_arg.back() == ' ')) {
                            script_arg += " " + std::string(argv[++i]);
                        }
                    }
                } else {
                    opts.files.push_back(arg);
                }
            }
        }

        if (!script_arg.empty()) {
            opts.scripts.push_back(script_arg);
        }

        if (opts.scripts.empty()) {
            std::cerr << "sed: no script or expression specified.\nTry 'sed --help' for more information.\n";
            return false;
        }

        return true;
    }
};

class SedApplication {
private:
    OptionParser m_parser;
    ScriptParser m_scriptParser;

public:
    int Run(int argc, char* argv[]) {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
#endif
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        SedOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 2;
        }
        if (exitEarly) {
            return 0;
        }

        opts.files.insert(opts.files.end(), opts.object_sources.begin(), opts.object_sources.end());

        std::vector<SedCommand> compiled_commands;
        for (const auto& s : opts.scripts) {
            auto cmds = m_scriptParser.parse(s);
            compiled_commands.insert(compiled_commands.end(), cmds.begin(), cmds.end());
        }

        SedEngine engine(compiled_commands, opts);

        if (opts.files.empty()) {
            engine.ProcessStream(std::cin, std::cout);
        } else {
            for (const auto& filepath : opts.files) {
                if (filepath.rfind("registry:", 0) == 0 || filepath.rfind("wmi:", 0) == 0) {
                    std::string content, error;
                    if (!SedWindowsObjectLoader::LoadObjectInput(filepath, content, error)) {
                        std::cerr << "sed: " << error << "\n";
                        continue;
                    }
                    std::istringstream input(std::move(content));
                    engine.ProcessStream(input, std::cout);
                    continue;
                }
                if (opts.in_place) {
                    std::error_code ec;
                    fs::file_status st = fs::status(filepath, ec);
                    if (ec || !fs::exists(st) || !fs::is_regular_file(st)) {
                        std::cerr << "sed: cannot operate on non-regular file " << filepath << "\n";
                        continue;
                    }
                    if (fs::symlink_status(filepath, ec).type() == fs::file_type::symlink) {
                        if (!opts.follow_symlinks) {
                            std::cerr << "sed: refusing to modify symlink " << filepath << "\n";
                            continue;
                        }
                    }

                    std::ifstream in(filepath, std::ios::binary);
                    if (!in.is_open()) {
                        std::cerr << "sed: cannot open file " << filepath << "\n";
                        continue;
                    }

                    std::string temp_path = filepath + ".tmp." + std::to_string(std::hash<std::string>{}(filepath));
                    std::ofstream out(temp_path, std::ios::binary);
                    if (!out.is_open()) {
                        std::cerr << "sed: cannot create temporary file for " << filepath << "\n";
                        continue;
                    }

                    engine.ProcessStream(in, out);
                    in.close();
                    out.close();

                    if (!opts.backup_suffix.empty()) {
                        std::string backup_path = filepath + opts.backup_suffix;
                        fs::copy_file(filepath, backup_path, fs::copy_options::overwrite_existing, ec);
                        if (ec) {
                            std::cerr << "sed: cannot create backup file " << backup_path << ": " << ec.message() << "\n";
                        }
                    }
                    ec.clear();
                    fs::copy_file(temp_path, filepath, fs::copy_options::overwrite_existing, ec);
                    if (ec) {
                        std::cerr << "sed: cannot overwrite " << filepath << ": " << ec.message() << "\n";
                    }
                    fs::remove(temp_path, ec);
                } else {
                    std::ifstream in(filepath, std::ios::binary);
                    if (!in.is_open()) {
                        std::cerr << "sed: cannot open file " << filepath << "\n";
                        continue;
                    }
                    engine.ProcessStream(in, std::cout);
                }
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    SedApplication app;
    return app.Run(argc, argv);
}