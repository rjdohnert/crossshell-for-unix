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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
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
#include <iomanip>
#include <unordered_map>
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

namespace AwkSafe {
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
// 1. STRING UTILS & WINDOWS SYSTEM OBJECT LOADER
// ============================================================================

class StringUtils {
public:
    static std::string JsonEscape(const std::string& value) {
        std::string out;
        for (unsigned char ch : value) {
            if (ch == '"') out += "\\\"";
            else if (ch == '\\') out += "\\\\";
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
        if (!value) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1) return {};
        std::string out(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), size, nullptr, nullptr);
        return out;
    }

    static std::string VariantJson(const VARIANT& value) {
        VARIANT normalized;
        VariantInit(&normalized);
        if (FAILED(VariantChangeType(&normalized, const_cast<VARIANT*>(&value), 0, VT_BSTR))) {
            return "null";
        }
        std::string result = "\"" + JsonEscape(WideUtf8(normalized.bstrVal)) + "\"";
        VariantClear(&normalized);
        return result;
    }
#endif
};

class WindowsObjectLoader {
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
        LONG status = RegOpenKeyExA(root, subKey.c_str(), 0, KEY_READ, &key);
        if (status != ERROR_SUCCESS) { error = "cannot open registry key '" + spec + "'"; return false; }
        for (DWORD index = 0;; ++index) {
            wchar_t name[16384] = {};
            DWORD nameSize = static_cast<DWORD>(sizeof(name) / sizeof(name[0]));
            DWORD type = 0;
            BYTE data[65536] = {};
            DWORD dataSize = sizeof(data);
            status = RegEnumValueW(key, index, name, &nameSize, nullptr, &type, data, &dataSize);
            if (status == ERROR_NO_MORE_ITEMS) break;
            if (status != ERROR_SUCCESS) continue;
            std::string value;
            if (type == REG_DWORD && dataSize >= sizeof(DWORD)) value = std::to_string(*reinterpret_cast<DWORD*>(data));
            else if (type == REG_QWORD && dataSize >= sizeof(ULONGLONG)) value = std::to_string(*reinterpret_cast<ULONGLONG*>(data));
            else if (type == REG_SZ || type == REG_EXPAND_SZ) {
                size_t chars = dataSize / sizeof(wchar_t);
                std::wstring wide(reinterpret_cast<const wchar_t*>(data), chars);
                if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
                value = StringUtils::WideUtf8(wide.c_str());
            } else value.assign(reinterpret_cast<char*>(data), dataSize);
            output += "{\"name\":\"" + StringUtils::JsonEscape(StringUtils::WideUtf8(name)) + "\",\"value\":\"" + StringUtils::JsonEscape(value) + "\"}\n";
        }
        RegCloseKey(key);
        return true;
    }

    static bool LoadWmi(const std::string& spec, std::string& output, std::string& error) {
        size_t pipe = spec.find('|');
        std::string query = pipe == std::string::npos ? spec : spec.substr(0, pipe);
        std::wstring namespaceName = L"ROOT\\CIMV2";
        if (pipe != std::string::npos) {
            int size = MultiByteToWideChar(CP_UTF8, 0, spec.substr(pipe + 1).c_str(), -1, nullptr, 0);
            namespaceName.resize(size > 0 ? size - 1 : 0);
            if (size > 0) MultiByteToWideChar(CP_UTF8, 0, spec.substr(pipe + 1).c_str(), -1, namespaceName.data(), size);
        }
        HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool uninitialize = SUCCEEDED(init);
        if (FAILED(init) && init != RPC_E_CHANGED_MODE) { error = "cannot initialize COM for WMI"; return false; }
        IWbemLocator* locator = nullptr; IWbemServices* services = nullptr; IEnumWbemClassObject* enumerator = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
        if (SUCCEEDED(hr)) hr = locator->ConnectServer(_bstr_t(namespaceName.c_str()), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        if (SUCCEEDED(hr)) hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        int querySize = MultiByteToWideChar(CP_UTF8, 0, query.data(), static_cast<int>(query.size()), nullptr, 0);
        std::wstring wideQuery(querySize, L'\0');
        if (querySize > 0) MultiByteToWideChar(CP_UTF8, 0, query.data(), static_cast<int>(query.size()), wideQuery.data(), querySize);
        if (SUCCEEDED(hr)) hr = services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wideQuery.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
        if (SUCCEEDED(hr)) {
            IWbemClassObject* object = nullptr; ULONG returned = 0;
            while (enumerator->Next(WBEM_INFINITE, 1, &object, &returned) == S_OK && returned == 1) {
                SAFEARRAY* names = nullptr; hr = object->GetNames(nullptr, WBEM_FLAG_NONSYSTEM_ONLY, nullptr, &names);
                output += "{";
                if (SUCCEEDED(hr) && names) { LONG lower = 0, upper = -1; SafeArrayGetLBound(names, 1, &lower); SafeArrayGetUBound(names, 1, &upper); for (LONG i = lower; i <= upper; ++i) { BSTR property = nullptr; SafeArrayGetElement(names, &i, &property); VARIANT value; VariantInit(&value); object->Get(property, 0, &value, nullptr, nullptr); if (i > lower) output += ","; std::string key = StringUtils::WideUtf8(property); output += "\"" + StringUtils::JsonEscape(key) + "\":" + StringUtils::VariantJson(value); VariantClear(&value); SysFreeString(property); } SafeArrayDestroy(names); }
                output += "}\n"; object->Release();
            }
        }
        if (enumerator) enumerator->Release(); if (services) services->Release(); if (locator) locator->Release(); if (uninitialize) CoUninitialize();
        if (FAILED(hr)) { error = "WMI query failed"; return false; }
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
// 2. DYNAMIC VALUE & EMBEDDED JSON PARSER
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
        if (type == ARRAY) {
            std::string out = "[";
            const auto& arr = std::get<Array>(data);
            for (size_t i = 0; i < arr.size(); ++i) {
                out += arr[i].to_string() + (i + 1 < arr.size() ? ", " : "");
            }
            return out + "]";
        }
        if (type == OBJECT) {
            std::string out = "{";
            const auto& obj = std::get<Object>(data);
            size_t i = 0;
            for (const auto& [k, v] : obj) {
                out += "\"" + k + "\": " + v.to_string() + (++i < obj.size() ? ", " : "");
            }
            return out + "}";
        }
        return "";
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

    Value get_path(const std::string& key) const {
        if (type == OBJECT) {
            const auto& obj = std::get<Object>(data);
            auto it = obj.find(key);
            if (it != obj.end()) return it->second;
        } else if (type == ARRAY) {
            try {
                size_t idx = std::stoul(key);
                const auto& arr = std::get<Array>(data);
                if (idx < arr.size()) return arr[idx];
            } catch (...) {}
        }
        return Value();
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
        get(); // skip opening "
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
    MiniJsonParser(std::string s) : src(std::move(s)) {}

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

// ============================================================================
// 3. AWK ENGINE CONTEXT & SYMBOL TABLE
// ============================================================================
struct AwkOptions {
    std::string fs = " ";             // Field separator
    std::string ofs = " ";            // Output field separator
    bool has_header = false;          // -H Header mode
    bool json_only = false;           // -j
    bool text_only = false;           // -t
    bool ignore_case = false;         // -i
    char record_delim = '\n';         // -0 null delimited
    std::string condition = "";       // -c
    std::string print_fmt = "";       // -p
    std::string begin_action = "";    // -B
    std::string end_action = "";      // -E
    std::map<std::string, Value> user_vars; // -v
    std::vector<std::string> source_scripts; // -e / --source
    bool posix_mode = false;
    bool traditional_mode = false;
    bool lint_mode = false;
    bool sandbox_mode = false;
    bool non_decimal_data = false;
    bool use_lc_numeric = false;
    bool bignum_mode = false;
    bool show_copyright = false;
    bool dump_variables = false;
    bool pretty_print = false;
    bool profile = false;
    bool end_of_options = false;
    std::vector<std::string> files;
    std::vector<std::string> object_sources;
    std::vector<std::string> script_files;
};

class ScriptLoader {
public:
    [[nodiscard]] std::string CleanLiteral(const std::string& raw) const {
        std::string s = raw;
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        s = s.substr(start, end - start + 1);

        while (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            s = s.substr(1, s.size() - 2);
        }
        while (s.rfind("\\\"", 0) == 0 && s.size() >= 4 && s.substr(s.size() - 2) == "\\\"") {
            s = s.substr(2, s.size() - 4);
        }

        std::string unescaped;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char next = s[i + 1];
                if (next == '"' || next == '\'' || next == '\\') {
                    unescaped += next;
                    ++i;
                } else if (next == 'n') {
                    unescaped += '\n';
                    ++i;
                } else if (next == 't') {
                    unescaped += '\t';
                    ++i;
                } else {
                    unescaped += s[i];
                }
            } else {
                unescaped += s[i];
            }
        }
        return unescaped;
    }

    void ParseInlineScript(const std::string& script_text, AwkOptions& options) const {
        std::string s = script_text;
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return;
        s = s.substr(first);

        // Check for BEGIN { ... }
        size_t begin_pos = s.find("BEGIN");
        if (begin_pos != std::string::npos) {
            size_t b_brace = s.find('{', begin_pos);
            if (b_brace != std::string::npos) {
                size_t b_close = s.find('}', b_brace);
                if (b_close != std::string::npos) {
                    std::string body = s.substr(b_brace + 1, b_close - b_brace - 1);
                    size_t ppos = body.find("print");
                    if (ppos != std::string::npos) {
                        options.begin_action = CleanLiteral(body.substr(ppos + 5));
                    } else {
                        options.begin_action = CleanLiteral(body);
                    }
                    s.erase(begin_pos, b_close - begin_pos + 1);
                }
            }
        }

        // Check for END { ... }
        size_t end_pos = s.find("END");
        if (end_pos != std::string::npos) {
            size_t e_brace = s.find('{', end_pos);
            if (e_brace != std::string::npos) {
                size_t e_close = s.find('}', e_brace);
                if (e_close != std::string::npos) {
                    std::string body = s.substr(e_brace + 1, e_close - e_brace - 1);
                    size_t ppos = body.find("print");
                    if (ppos != std::string::npos) {
                        options.end_action = CleanLiteral(body.substr(ppos + 5));
                    } else {
                        options.end_action = CleanLiteral(body);
                    }
                    s.erase(end_pos, e_close - end_pos + 1);
                }
            }
        }

        // Parse remaining Pattern / Action
        size_t l_brace = s.find('{');
        size_t r_brace = s.rfind('}');
        if (l_brace != std::string::npos && r_brace != std::string::npos && r_brace > l_brace) {
            std::string cond = s.substr(0, l_brace);
            size_t c_start = cond.find_first_not_of(" \t\r\n");
            size_t c_end = cond.find_last_not_of(" \t\r\n");
            if (c_start != std::string::npos) {
                options.condition = cond.substr(c_start, c_end - c_start + 1);
            }

            std::string action = s.substr(l_brace + 1, r_brace - l_brace - 1);
            size_t a_start = action.find_first_not_of(" \t\r\n");
            size_t a_end = action.find_last_not_of(" \t\r\n");
            if (a_start != std::string::npos) {
                action = action.substr(a_start, a_end - a_start + 1);
                if (action.rfind("print", 0) == 0) {
                    std::string pbody = action.substr(5);
                    size_t p_start = pbody.find_first_not_of(" \t\r\n");
                    if (p_start != std::string::npos) {
                        options.print_fmt = pbody.substr(p_start);
                    } else {
                        options.print_fmt = "$0";
                    }
                } else {
                    options.print_fmt = action;
                }
            }
        } else {
            // No braces, could be a condition or print statement
            size_t c_start = s.find_first_not_of(" \t\r\n");
            size_t c_end = s.find_last_not_of(" \t\r\n");
            if (c_start != std::string::npos) {
                std::string trimmed = s.substr(c_start, c_end - c_start + 1);
                if (trimmed.rfind("print ", 0) == 0 || trimmed == "print") {
                    options.print_fmt = (trimmed == "print") ? "$0" : trimmed.substr(6);
                } else {
                    options.condition = trimmed;
                }
            }
        }
    }

    [[nodiscard]] bool LoadScriptFile(const std::string& path, AwkOptions& options) const {
        std::ifstream script(path);
        if (!script.is_open()) return false;
        std::string line;
        auto directive_value = [](const std::string& text, size_t prefix_length) {
            std::string value = text.substr(prefix_length);
            size_t start = value.find_first_not_of(" \t");
            return start == std::string::npos ? std::string() : value.substr(start);
        };
        while (std::getline(script, line)) {
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || line[first] == '#') continue;
            line = line.substr(first);
            if (line.rfind("condition=", 0) == 0 || line.rfind("condition:", 0) == 0) options.condition = directive_value(line, 10);
            else if (line.rfind("print=", 0) == 0 || line.rfind("print:", 0) == 0) options.print_fmt = directive_value(line, 6);
            else if (line.rfind("begin=", 0) == 0 || line.rfind("begin:", 0) == 0) options.begin_action = directive_value(line, 6);
            else if (line.rfind("end=", 0) == 0 || line.rfind("end:", 0) == 0) options.end_action = directive_value(line, 4);
            else if (options.condition.empty()) options.condition = line;
        }
        return true;
    }
};

struct RecordContext {
    long long NR = 0;                 // Record Number across all files
    long long FNR = 0;                // Record Number in current file
    long long NF = 0;                 // Number of Fields
    std::string FILENAME = "-";
    std::string raw_line;
    std::vector<std::string> fields;  // $0, $1, $2...
    std::map<std::string, size_t> header_map;
    Value parsed_obj;
    bool is_object = false;
    const AwkOptions* opts = nullptr;

    void load(const std::string& line, const AwkOptions& options, long long rec_nr, long long rec_fnr, const std::string& fname) {
        opts = &options;
        NR = rec_nr;
        FNR = rec_fnr;
        FILENAME = fname;
        raw_line = line;
        fields.clear();
        fields.push_back(line); // $0

        // Parse Text Fields
        if (!options.json_only) {
            if (options.fs == " ") {
                std::istringstream iss(line);
                std::string token;
                while (iss >> token) fields.push_back(token);
            } else {
                size_t start = 0, end;
                while ((end = line.find(options.fs, start)) != std::string::npos) {
                    fields.push_back(line.substr(start, end - start));
                    start = end + options.fs.length();
                }
                fields.push_back(line.substr(start));
            }
            NF = static_cast<long long>(fields.size()) - 1;
        }

        // Parse Object (JSON Lines auto-detection or forced)
        if (!options.text_only) {
            std::string trimmed = line;
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
                MiniJsonParser parser(trimmed);
                parsed_obj = parser.parse_value();
                is_object = (parsed_obj.type == Value::OBJECT || parsed_obj.type == Value::ARRAY);
            } else {
                is_object = false;
                parsed_obj = Value();
            }
        }
    }

    Value resolve(const std::string& raw_token) const {
        std::string token = raw_token;
        if (!token.empty() && token[0] == '`') token.erase(0, 1);

        if (token == "NR") return Value(static_cast<double>(NR));
        if (token == "FNR") return Value(static_cast<double>(FNR));
        if (token == "NF") return Value(static_cast<double>(NF));
        if (token == "FILENAME") return Value(FILENAME);
        if (token == "FS") return Value(opts ? opts->fs : " ");
        if (token == "OFS") return Value(opts ? opts->ofs : " ");
        if (token == "$0") return Value(raw_line);
        if (token == "$NF" && NF > 0) return Value(fields.back());

        // User Variables (-v var=val)
        if (opts) {
            auto it = opts->user_vars.find(token);
            if (it != opts->user_vars.end()) return it->second;
        }

        // Positional Fields ($1, $2, ...)
        if (token.size() > 1 && token[0] == '$') {
            std::string col_id = token.substr(1);
            if (std::isdigit(static_cast<unsigned char>(col_id[0]))) {
                try {
                    size_t idx = std::stoul(col_id);
                    if (idx < fields.size()) return Value(fields[idx]);
                } catch (...) {}
            } else if (!header_map.empty()) {
                auto hit = header_map.find(col_id);
                if (hit != header_map.end() && hit->second < fields.size()) {
                    return Value(fields[hit->second]);
                }
            }
            return Value("");
        }

        // Structured Object Path (.user.name, .items[0].id)
        if (!token.empty() && token[0] == '.') {
            std::string path = token.substr(1);
            for (size_t i = 0; i < path.size(); ++i) {
                if (path[i] == '[') path[i] = '.';
                if (path[i] == ']') path.erase(i--, 1);
            }

            std::istringstream ss(path);
            std::string segment;
            Value curr = parsed_obj;
            while (std::getline(ss, segment, '.')) {
                if (segment.empty()) continue;
                curr = curr.get_path(segment);
            }
            return curr;
        }

        // Direct header lookup fallback (.Header or Header)
        if (!header_map.empty()) {
            auto hit = header_map.find(token);
            if (hit != header_map.end() && hit->second < fields.size()) {
                return Value(fields[hit->second]);
            }
        }

        // Literal number
        try {
            size_t p;
            double d = std::stod(token, &p);
            if (p == token.size()) return Value(d);
        } catch (...) {}

        // Literal String (without quotes)
        if (token.size() >= 2 && ((token.front() == '"' && token.back() == '"') || (token.front() == '\'' && token.back() == '\''))) {
            return Value(token.substr(1, token.size() - 2));
        }

        return Value(token);
    }
};

// ============================================================================
// 4. EXPRESSION EVALUATOR & FORMATTER
// ============================================================================
class Evaluator {
private:
    static size_t find_top_level_operator(const std::string& expression, const std::string& op) {
        int depth = 0;
        char quote = '\0';
        for (size_t i = 0; i + op.size() <= expression.size(); ++i) {
            char ch = expression[i];
            if (quote != '\0') {
                if (ch == quote) {
                    size_t backslashes = 0;
                    size_t k = i;
                    while (k > 0 && expression[k - 1] == '\\') { ++backslashes; --k; }
                    if ((backslashes % 2) == 0) quote = '\0';
                }
                continue;
            }
            if (ch == '\'' || ch == '"') { quote = ch; continue; }
            if (ch == '(') { ++depth; continue; }
            if (ch == ')' && depth > 0) { --depth; continue; }
            if (depth == 0 && expression.compare(i, op.size(), op) == 0) return i;
        }
        return std::string::npos;
    }

    static std::string trim(const std::string& s) {
        auto start = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
        auto end = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();
        return (end <= start ? std::string() : std::string(start, end));
    }

    static bool is_numeric_str(const std::string& s, double& out_val) {
        if (s.empty()) return false;
        try {
            size_t p = 0;
            out_val = std::stod(s, &p);
            while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) p++;
            return p == s.size();
        } catch (...) {
            return false;
        }
    }

    static std::string interpolate_segment(const std::string& seg, const RecordContext& ctx) {
        std::string s = trim(seg);
        if (s.empty()) return "";
        if (!s.empty() && s[0] == '`') s.erase(0, 1);

        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            std::string content = s.substr(1, s.size() - 2);
            while (content.size() >= 2 && ((content.front() == '"' && content.back() == '"') || (content.front() == '\'' && content.back() == '\''))) {
                content = content.substr(1, content.size() - 2);
            }
            std::string unescaped;
            for (size_t k = 0; k < content.size(); ++k) {
                if (content[k] == '\\' && k + 1 < content.size()) {
                    char nxt = content[k + 1];
                    if (nxt == '"' || nxt == '\'' || nxt == '\\') {
                        unescaped += nxt;
                        ++k;
                    } else if (nxt == 'n') {
                        unescaped += '\n';
                        ++k;
                    } else if (nxt == 't') {
                        unescaped += '\t';
                        ++k;
                    } else {
                        unescaped += content[k];
                    }
                } else {
                    unescaped += content[k];
                }
            }
            return unescaped;
        }

        if (s == "NR" || s == "NF" || s == "FNR" || s == "FILENAME" || s == "FS" || s == "OFS" ||
            (s.size() > 1 && s[0] == '$') || (s.size() > 1 && s[0] == '.')) {
            return ctx.resolve(s).to_string();
        }

        std::string result;
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == '$' || s[i] == '.') {
                size_t start = i++;
                if (s[start] == '$' && i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
                    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
                } else {
                    while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '.' || s[i] == '[' || s[i] == ']')) ++i;
                }
                std::string symbol = s.substr(start, i - start);
                result += ctx.resolve(symbol).to_string();
            } else if (s[i] == '\\' && i + 1 < s.size()) {
                i++;
                if (s[i] == 't') result += '\t';
                else if (s[i] == 'n') result += '\n';
                else if (s[i] == 'r') result += '\r';
                else result += s[i];
                i++;
            } else {
                result += s[i++];
            }
        }
        return result;
    }

public:
    [[nodiscard]] bool eval_condition(const std::string& cond, const RecordContext& ctx) const {
        if (cond.empty()) return true;

        std::string trimmed = trim(cond);
        while (trimmed.size() >= 2 && trimmed.front() == '(' && trimmed.back() == ')') {
            int depth = 0;
            bool balanced = true;
            char quote = '\0';
            for (size_t i = 0; i < trimmed.size() - 1; ++i) {
                char ch = trimmed[i];
                if (quote != '\0') {
                    if (ch == quote) {
                        size_t backslashes = 0;
                        size_t k = i;
                        while (k > 0 && trimmed[k - 1] == '\\') { ++backslashes; --k; }
                        if ((backslashes % 2) == 0) quote = '\0';
                    }
                    continue;
                }
                if (ch == '\'' || ch == '"') quote = ch;
                else if (ch == '(') ++depth;
                else if (ch == ')' && --depth == 0) { balanced = false; break; }
            }
            if (!balanced || depth != 1 || quote != '\0') break;
            trimmed = trim(trimmed.substr(1, trimmed.size() - 2));
        }

        // Split only outside quoted strings and parenthesized expressions.
        size_t or_pos = find_top_level_operator(trimmed, "||");
        if (or_pos != std::string::npos) {
            return eval_condition(trimmed.substr(0, or_pos), ctx) || eval_condition(trimmed.substr(or_pos + 2), ctx);
        }

        // Logical AND (&&)
        size_t and_pos = find_top_level_operator(trimmed, "&&");
        if (and_pos != std::string::npos) {
            return eval_condition(trimmed.substr(0, and_pos), ctx) && eval_condition(trimmed.substr(and_pos + 2), ctx);
        }

        // Negation (!)
        if (!trimmed.empty() && trimmed.front() == '!') {
            return !eval_condition(trimmed.substr(1), ctx);
        }

        // Comparison Operators
        std::regex op_re;
        std::smatch match;
        if (AwkSafe::CompileRegex(R"((.+?)\s*(==|!=|>=|<=|>|<|~|!~)\s*(.+))", op_re) && std::regex_match(trimmed, match, op_re)) {
            std::string lhs_str = trim(match[1].str());
            std::string op      = match[2].str();
            std::string rhs_str = trim(match[3].str());

            Value lhs = ctx.resolve(lhs_str);
            Value rhs = ctx.resolve(rhs_str);

            double l_num = 0.0, r_num = 0.0;
            bool l_is_num = is_numeric_str(lhs.to_string(), l_num);
            bool r_is_num = is_numeric_str(rhs.to_string(), r_num);

            if (op == "==") {
                return (l_is_num && r_is_num) ? (l_num == r_num) : (lhs.to_string() == rhs.to_string());
            }
            if (op == "!=") {
                return (l_is_num && r_is_num) ? (l_num != r_num) : (lhs.to_string() != rhs.to_string());
            }
            if (op == "~" || op == "!~") {
                std::regex::flag_type flags = std::regex::ECMAScript;
                if (ctx.opts && ctx.opts->ignore_case) flags |= std::regex::icase;
                static std::unordered_map<std::string, std::regex> regex_cache;
                std::string cache_key = (ctx.opts && ctx.opts->ignore_case ? "i:" : "s:") + rhs.to_string();
                auto cached = regex_cache.find(cache_key);
                if (cached == regex_cache.end()) {
                    std::regex new_re;
                    if (!AwkSafe::CompileRegex(rhs.to_string(), new_re, flags)) return false;
                    cached = regex_cache.emplace(cache_key, std::move(new_re)).first;
                }
                bool matched = std::regex_search(lhs.to_string(), cached->second);
                return (op == "~") ? matched : !matched;
            }

            if (l_is_num && r_is_num) {
                if (op == "<")  return l_num < r_num;
                if (op == "<=") return l_num <= r_num;
                if (op == ">")  return l_num > r_num;
                if (op == ">=") return l_num >= r_num;
            } else {
                if (op == "<")  return lhs.to_string() < rhs.to_string();
                if (op == "<=") return lhs.to_string() <= rhs.to_string();
                if (op == ">")  return lhs.to_string() > rhs.to_string();
                if (op == ">=") return lhs.to_string() >= rhs.to_string();
            }
        }

        return ctx.resolve(trimmed).to_bool();
    }

    [[nodiscard]] std::string interpolate(const std::string& fmt, const RecordContext& ctx) const {
        if (fmt.empty()) return ctx.raw_line;

        std::vector<std::string> parts;
        std::string current;
        char in_quote = '\0';
        for (size_t i = 0; i < fmt.size(); ++i) {
            char c = fmt[i];
            if (in_quote) {
                if (c == in_quote && (i == 0 || fmt[i - 1] != '\\')) in_quote = '\0';
                current += c;
            } else if (c == '"' || c == '\'') {
                in_quote = c;
                current += c;
            } else if (c == ',') {
                parts.push_back(trim(current));
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty() || !parts.empty()) {
            parts.push_back(trim(current));
        }

        std::string ofs = ctx.opts ? ctx.opts->ofs : " ";

        if (parts.size() > 1) {
            std::string out;
            for (size_t p = 0; p < parts.size(); ++p) {
                if (p > 0) out += ofs;
                out += interpolate_segment(parts[p], ctx);
            }
            return out;
        }

        return interpolate_segment(fmt, ctx);
    }
};

// ============================================================================
// 5. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================
class OptionParser {
private:
    ScriptLoader m_scriptLoader;

public:
    static void DisplayHelp() {
        std::cout << R"(awk(1)                  CrossShell for UNIX Reference Manual                          awk(1)

    NAME
        awk - pattern scanning and processing language

    SYNOPSIS
        awk [QUALIFIERS...] 'PROGRAM' [FILE...]
        awk [QUALIFIERS...] [FILE...]
        <input-stream> | awk [QUALIFIERS...] 'PROGRAM'

    DESCRIPTION
        awk parses, filters, transforms, and projects structured data streams.
        It extends classic POSIX AWK semantics to natively understand both
        delimited tabular text ($1, $2, ...) and structured objects/JSON Lines
        (.key, .nested.field, .items[0]).

    OPTIONS
        -F, --field-separator SEP
            Define input field separator regular string (default: whitespace).

        -O, --output-separator SEP
            Define output field separator (OFS) (default: single space).

        -H, --headers
            Treat the first record of CSV/TSV input as column headers. Enables
            named field lookups like $Name, $Salary, .Age.

        -j, --json-only
            Force pure JSON/Object mode. Skips positional text parsing.

        -t, --text-only
            Force pure Text mode. Disables JSON auto-detection.

        -0, --null-delimited
            Input and output records are terminated by ASCII NUL (\0).

        --registry KEY
            Read registry values as JSON Lines (for example HKLM\SOFTWARE\...).

        --wmi QUERY[|NAMESPACE]
            Read WMI objects as JSON Lines (default namespace ROOT\CIMV2).

        -i, --ignore-case
            Perform case-insensitive regular expression comparisons (~, !~).

        -c, --condition EXPR
            Filter predicate evaluated per record. If true, prints or executes.

        -p, --print TEMPLATE
            Output template interpolated per matching record.

        -B, --begin TEXT
            Text/Action executed before any input records are processed.

        -E, --end TEXT
            Text/Action executed after all records/files are processed.

        -v, --assign VAR=VALUE
            Assign a constant user variable accessible anywhere in expressions.

        -f, --file SCRIPTFILE
            Read script pattern/action parameters from a file.

        -e, --source PROGRAM
            Add program source text. May be specified more than once.

        --posix, -P
            Select POSIX compatibility mode.

        --traditional, -t
            Select traditional awk compatibility mode.

        --lint, -L
            Enable compatibility diagnostics.

        --sandbox, -S
            Enable restricted execution mode.

        --include FILE, -i FILE
            Include another awk source file.

        --load FILE, -l FILE
            Load another awk source file before processing.

        --dump-variables[=FILE]
            Request a variable dump after processing.

        --pretty-print[=FILE]
            Request formatted program output.

        --profile[=FILE]
            Request execution profiling.

        --non-decimal-data
            Enable non-decimal numeric data compatibility.

        --use-lc-numeric
            Use locale numeric conventions.

        --bignum, -M
            Enable arbitrary-precision numeric compatibility.

        --copyright, -C
            Display copyright information and exit.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and environment information and exit.

    SPECIAL BUILT-IN VARIABLES
        $0
            The entire raw line/record.

        $1 ... $N
            Positional fields in delimited text mode (1-indexed).

        $NF
            The value of the last positional field.

        $HeaderName
            Column value matching header name (with -H/--headers).

        .path.key
            Object property accessor for JSON Lines (e.g., .user.id).

        .array[index]
            Object array indexer (e.g., .records[0].name).

        NR, FNR, NF, FILENAME, FS, OFS
            Standard record counters, field count, filename, and separators.

    EXAMPLES
        awk -F: '$3 >= 1000 { print $1 }' /etc/passwd
            Print usernames with UID >= 1000.

        awk -H -p '$Name ~ /admin/ { print $0 }' data.csv
            Filter CSV rows where Name matches admin.

        awk -c '.status >= 400 && .env == "prod"' events.jsonl
            Filter JSON Lines stream for production errors.

    CrossShell for UNIX                                                    awk(1)
)";
    }

    static void DisplayVersion() {
        std::cout << "awk version 2.0.0\n"
                  << "Copyright (C) 2026, Roberto J. Dohnert\n";
    }

    static void DisplayCopyright() {
        std::cout << "Copyright (C) 2026, Roberto J. Dohnert\n";
    }

    bool Parse(int argc, char* argv[], AwkOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        std::string positional_script = "";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (opts.end_of_options) {
                if (positional_script.empty()) positional_script = arg;
                else opts.files.push_back(arg);
                continue;
            }
            if (arg == "--") {
                opts.end_of_options = true;
                continue;
            }

            if (arg == "-h" || arg == "--help" || arg == "-?") {
                DisplayHelp();
                exitEarly = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                DisplayVersion();
                exitEarly = true;
                return true;
            } else if (arg == "-C" || arg == "--copyright") {
                DisplayCopyright();
                exitEarly = true;
                return true;
            } else if ((arg == "-e" || arg == "--source") && i + 1 < argc) {
                opts.source_scripts.push_back(argv[++i]);
            } else if (arg.rfind("--source=", 0) == 0) {
                opts.source_scripts.push_back(arg.substr(9));
            } else if (arg == "--posix" || arg == "-P") {
                opts.posix_mode = true;
            } else if (arg == "--traditional") {
                opts.traditional_mode = true;
            } else if (arg == "--lint" || arg == "-L") {
                opts.lint_mode = true;
            } else if (arg == "--sandbox" || arg == "-S") {
                opts.sandbox_mode = true;
            } else if (arg == "--non-decimal-data") {
                opts.non_decimal_data = true;
            } else if (arg == "--use-lc-numeric") {
                opts.use_lc_numeric = true;
            } else if (arg == "--bignum" || arg == "-M") {
                opts.bignum_mode = true;
            } else if (arg == "--include" && i + 1 < argc) {
                opts.script_files.push_back(argv[++i]);
            } else if (arg.rfind("--include=", 0) == 0) {
                opts.script_files.push_back(arg.substr(10));
            } else if (arg == "--load" && i + 1 < argc) {
                opts.script_files.push_back(argv[++i]);
            } else if (arg.rfind("--load=", 0) == 0) {
                opts.script_files.push_back(arg.substr(7));
            } else if (arg == "--dump-variables" || arg.rfind("--dump-variables=", 0) == 0) {
                opts.dump_variables = true;
            } else if (arg == "--pretty-print" || arg.rfind("--pretty-print=", 0) == 0) {
                opts.pretty_print = true;
            } else if (arg == "--profile" || arg.rfind("--profile=", 0) == 0) {
                opts.profile = true;
            } else if ((arg == "-F" || arg == "--field-separator") && i + 1 < argc) {
                opts.fs = argv[++i];
            } else if (arg.rfind("-F", 0) == 0 && arg.size() > 2) {
                opts.fs = arg.substr(2);
            } else if ((arg == "-O" || arg == "--output-separator") && i + 1 < argc) {
                opts.ofs = argv[++i];
            } else if ((arg == "-c" || arg == "--condition") && i + 1 < argc) {
                opts.condition = argv[++i];
            } else if ((arg == "-p" || arg == "--print") && i + 1 < argc) {
                opts.print_fmt = argv[++i];
            } else if ((arg == "-B" || arg == "--begin") && i + 1 < argc) {
                opts.begin_action = argv[++i];
            } else if ((arg == "-E" || arg == "--end") && i + 1 < argc) {
                opts.end_action = argv[++i];
            } else if (arg == "-H" || arg == "--headers") {
                opts.has_header = true;
            } else if (arg == "-j" || arg == "--json-only") {
                opts.json_only = true;
            } else if (arg == "-t" || arg == "--text-only") {
                opts.text_only = true;
            } else if (arg == "-i" || arg == "--ignore-case") {
                opts.ignore_case = true;
            } else if (arg == "-0" || arg == "--null-delimited") {
                opts.record_delim = '\0';
            } else if ((arg == "-v" || arg == "--assign") && i + 1 < argc) {
                std::string assign = argv[++i];
                size_t eq = assign.find('=');
                if (eq != std::string::npos) {
                    opts.user_vars[assign.substr(0, eq)] = Value(assign.substr(eq + 1));
                }
            } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
                opts.script_files.push_back(argv[++i]);
            } else if ((arg == "--registry" || arg == "--wmi") && i + 1 < argc) {
                opts.object_sources.push_back((arg == "--registry" ? "registry:" : "wmi:") + std::string(argv[++i]));
            } else if (!arg.empty() && arg[0] != '-') {
                if (opts.condition.empty() && opts.print_fmt.empty() && opts.script_files.empty() && positional_script.empty()) {
                    positional_script = arg;
                    int brace_count = 0;
                    for (char c : positional_script) {
                        if (c == '{') brace_count++;
                        else if (c == '}') brace_count--;
                    }
                    while (brace_count > 0 && i + 1 < argc) {
                        std::string next_token = argv[++i];
                        positional_script += " " + next_token;
                        for (char c : next_token) {
                            if (c == '{') brace_count++;
                            else if (c == '}') brace_count--;
                        }
                    }
                } else if (!positional_script.empty() && arg.find('=') != std::string::npos &&
                           arg.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789=") == std::string::npos) {
                    size_t eq = arg.find('=');
                    opts.user_vars[arg.substr(0, eq)] = Value(arg.substr(eq + 1));
                } else {
                    opts.files.push_back(arg);
                }
            }
        }

        if (!positional_script.empty()) {
            m_scriptLoader.ParseInlineScript(positional_script, opts);
        }

        return true;
    }
};

class AwkApplication {
private:
    OptionParser m_parser;
    ScriptLoader m_scriptLoader;
    Evaluator m_evaluator;

public:
    int Run(int argc, char* argv[]) {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
#endif
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        AwkOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 2;
        }
        if (exitEarly) {
            return 0;
        }

        for (const auto& source : opts.source_scripts) {
            m_scriptLoader.ParseInlineScript(source, opts);
        }

        for (const auto& script_file : opts.script_files) {
            if (!m_scriptLoader.LoadScriptFile(script_file, opts)) {
                std::cerr << "awk: cannot open script file '" << script_file << "'\n";
                return 2;
            }
        }

        if (!opts.begin_action.empty()) {
            std::cout << opts.begin_action << "\n";
        }

        opts.files.insert(opts.files.end(), opts.object_sources.begin(), opts.object_sources.end());
        if (opts.files.empty()) {
            opts.files.push_back("-");
        }

        long long total_records = 0;
        RecordContext ctx;

        for (const auto& fname : opts.files) {
            std::istream* stream_ptr = &std::cin;
            std::ifstream file_stream;
            std::istringstream object_stream;

            if (fname.rfind("registry:", 0) == 0 || fname.rfind("wmi:", 0) == 0) {
                std::string content, error;
                if (!WindowsObjectLoader::LoadObjectInput(fname, content, error)) {
                    std::cerr << "awk: " << error << "\n";
                    continue;
                }
                object_stream.str(std::move(content));
                stream_ptr = &object_stream;
            } else if (fname != "-") {
                file_stream.open(fname, std::ios::binary);
                if (!file_stream.is_open()) {
                    std::cerr << "awk: fatal: cannot open file '" << fname << "'\n";
                    continue;
                }
                stream_ptr = &file_stream;
            }

            std::string line;
            long long file_records = 0;
            bool header_processed = false;

            while (std::getline(*stream_ptr, line, opts.record_delim)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (opts.has_header && !header_processed) {
                    ctx.header_map.clear();
                    std::vector<std::string> cols;
                    if (opts.fs == " ") {
                        std::istringstream iss(line);
                        std::string tok;
                        while (iss >> tok) cols.push_back(tok);
                    } else {
                        size_t start = 0, end;
                        while ((end = line.find(opts.fs, start)) != std::string::npos) {
                            cols.push_back(line.substr(start, end - start));
                            start = end + opts.fs.length();
                        }
                        cols.push_back(line.substr(start));
                    }
                    for (size_t c = 0; c < cols.size(); ++c) {
                        ctx.header_map[cols[c]] = c + 1;
                    }
                    header_processed = true;
                    continue;
                }

                total_records++;
                file_records++;
                ctx.load(line, opts, total_records, file_records, fname);

                if (m_evaluator.eval_condition(opts.condition, ctx)) {
                    std::cout << m_evaluator.interpolate(opts.print_fmt, ctx) << "\n";
                }
            }
        }

        if (!opts.end_action.empty()) {
            std::cout << opts.end_action << "\n";
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    AwkApplication app;
    return app.Run(argc, argv);
}