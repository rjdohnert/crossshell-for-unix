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
#include <deque>
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
#include <io.h>
#else
#include <unistd.h>
#endif

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

namespace GrepSafe {
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

class GrepStringUtils {
public:
    static size_t TopLevelOperator(const std::string& expression, const std::string& op) {
        int depth = 0;
        char quote = '\0';
        for (size_t i = 0; i + op.size() <= expression.size(); ++i) {
            char ch = expression[i];
            if (quote != '\0') {
                if (ch == quote) {
                    size_t slashes = 0, k = i;
                    while (k > 0 && expression[k - 1] == '\\') { ++slashes; --k; }
                    if ((slashes % 2) == 0) quote = '\0';
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

    static std::string Trim(const std::string& value) {
        size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

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
        VARIANT converted;
        VariantInit(&converted);
        if (FAILED(VariantChangeType(&converted, const_cast<VARIANT*>(&value), 0, VT_BSTR))) return "null";
        std::string out = "\"" + JsonEscape(WideUtf8(converted.bstrVal)) + "\"";
        VariantClear(&converted);
        return out;
    }
#endif
};

class GrepWindowsObjectLoader {
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
        if (RegOpenKeyExA(root, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) { error = "cannot open registry key '" + spec + "'"; return false; }
        for (DWORD index = 0;; ++index) {
            wchar_t name[16384] = {};
            DWORD nameSize = static_cast<DWORD>(sizeof(name) / sizeof(name[0])), type = 0, dataSize = 65536;
            BYTE data[65536] = {};
            LONG status = RegEnumValueW(key, index, name, &nameSize, nullptr, &type, data, &dataSize);
            if (status == ERROR_NO_MORE_ITEMS) break;
            if (status != ERROR_SUCCESS) continue;
            std::string value;
            if (type == REG_DWORD && dataSize >= sizeof(DWORD)) value = std::to_string(*reinterpret_cast<DWORD*>(data));
            else if (type == REG_QWORD && dataSize >= sizeof(ULONGLONG)) value = std::to_string(*reinterpret_cast<ULONGLONG*>(data));
            else if (type == REG_SZ || type == REG_EXPAND_SZ) {
                size_t chars = dataSize / sizeof(wchar_t);
                std::wstring wide(reinterpret_cast<const wchar_t*>(data), chars);
                if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
                value = GrepStringUtils::WideUtf8(wide.c_str());
            }
            else value.assign(reinterpret_cast<char*>(data), dataSize);
            output += "{\"name\":\"" + GrepStringUtils::JsonEscape(GrepStringUtils::WideUtf8(name)) + "\",\"value\":\"" + GrepStringUtils::JsonEscape(value) + "\"}\n";
        }
        RegCloseKey(key);
        return true;
    }

    static bool LoadWmi(const std::string& spec, std::string& output, std::string& error) {
        size_t pipe = spec.find('|');
        std::string query = pipe == std::string::npos ? spec : spec.substr(0, pipe);
        std::wstring ns = L"ROOT\\CIMV2";
        if (pipe != std::string::npos) {
            std::string text = spec.substr(pipe + 1);
            int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            ns.resize(n > 0 ? n - 1 : 0);
            if (n > 0) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, ns.data(), n);
        }
        HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool uninit = SUCCEEDED(init);
        if (FAILED(init) && init != RPC_E_CHANGED_MODE) { error = "cannot initialize COM for WMI"; return false; }
        IWbemLocator* locator = nullptr; IWbemServices* services = nullptr; IEnumWbemClassObject* rows = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
        if (SUCCEEDED(hr)) hr = locator->ConnectServer(_bstr_t(ns.c_str()), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        if (SUCCEEDED(hr)) hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        int querySize = MultiByteToWideChar(CP_UTF8, 0, query.data(), static_cast<int>(query.size()), nullptr, 0);
        std::wstring wideQuery(querySize, L'\0');
        if (querySize > 0) MultiByteToWideChar(CP_UTF8, 0, query.data(), static_cast<int>(query.size()), wideQuery.data(), querySize);
        if (SUCCEEDED(hr)) hr = services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wideQuery.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &rows);
        if (SUCCEEDED(hr)) {
            IWbemClassObject* row = nullptr; ULONG count = 0;
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
                        output += "\"" + GrepStringUtils::JsonEscape(GrepStringUtils::WideUtf8(property)) + "\":" + GrepStringUtils::VariantJson(value);
                        VariantClear(&value);
                        SysFreeString(property);
                    }
                    SafeArrayDestroy(names);
                }
                output += "}\n";
                row->Release();
            }
        }
        if (rows) rows->Release(); if (services) services->Release(); if (locator) locator->Release(); if (uninit) CoUninitialize();
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
// 2. EMBEDDED JSON ENGINE
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
// 3. GREP OPTIONS & CONFIGURATION
// ============================================================================
struct GrepOptions {
    std::vector<std::string> patterns;
    std::string key_path = "";
    std::string where_expr = "";
    std::string extract_path = "";

    bool ignore_case = false;
    bool invert_match = false;
    bool fixed_strings = false;
    bool quiet = false;
    bool no_messages = false;
    bool text_mode = false;
    bool byte_offset = false;
    bool null_output = false;
    bool binary_files_without_match = false;
    bool binary_mode = false;
    bool word_regexp = false;
    bool line_regexp = false;
    bool line_number = false;
    bool count_only = false;
    bool files_with_matches = false;
    bool files_without_match = false;
    bool only_matching = false;
    bool with_filename = false;
    bool no_filename = false;
    bool recursive = false;
    bool json_mode = false;
    bool text_only = false;
    bool color = true;
    bool color_explicit = false;

    std::string directory_action = "read";
    std::string devices_action = "read";

    int max_count = -1;
    int before_context = 0;
    int after_context = 0;

    std::vector<std::string> file_paths;
    std::vector<std::string> object_sources;
};

const std::string COLOR_RESET   = "\033[0m";
const std::string COLOR_MATCH   = "\033[1;31m";
const std::string COLOR_FILE    = "\033[35m";
const std::string COLOR_LINE    = "\033[32m";
const std::string COLOR_SEP     = "\033[36m";

// ============================================================================
// 4. PREDICATE & PATH QUERY ENGINE
// ============================================================================
class ObjectEvaluator {
public:
    [[nodiscard]] Value resolve_path(const Value& root, const std::string& path_str) const {
        if (path_str.empty()) return root;
        std::string path = path_str;
        if (path.front() == '.') path = path.substr(1);

        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == '[') path[i] = '.';
            if (path[i] == ']') path.erase(i--, 1);
        }

        std::istringstream ss(path);
        std::string segment;
        Value curr = root;
        while (std::getline(ss, segment, '.')) {
            if (segment.empty()) continue;
            curr = curr.get_path(segment);
        }
        return curr;
    }

    [[nodiscard]] bool eval_expression(const std::string& expr, const Value& obj, const std::string& raw_line, bool ignore_case = false) const {
        if (expr.empty()) return true;

        std::string trimmed = GrepStringUtils::Trim(expr);
        while (trimmed.size() >= 2 && trimmed.front() == '(' && trimmed.back() == ')') {
            int depth = 0; bool balanced = true; char quote = '\0';
            for (size_t i = 0; i + 1 < trimmed.size(); ++i) {
                char ch = trimmed[i];
                if (quote != '\0') {
                    if (ch == quote) {
                        size_t slashes = 0, k = i;
                        while (k > 0 && trimmed[k - 1] == '\\') { ++slashes; --k; }
                        if ((slashes % 2) == 0) quote = '\0';
                    }
                    continue;
                }
                if (ch == '\'' || ch == '"') quote = ch;
                else if (ch == '(') ++depth;
                else if (ch == ')' && --depth == 0) { balanced = false; break; }
            }
            if (!balanced || depth != 1 || quote != '\0') break;
            trimmed = GrepStringUtils::Trim(trimmed.substr(1, trimmed.size() - 2));
        }

        size_t or_pos = GrepStringUtils::TopLevelOperator(trimmed, "||");
        if (or_pos != std::string::npos) {
            return eval_expression(trimmed.substr(0, or_pos), obj, raw_line, ignore_case) ||
                   eval_expression(trimmed.substr(or_pos + 2), obj, raw_line, ignore_case);
        }

        size_t and_pos = GrepStringUtils::TopLevelOperator(trimmed, "&&");
        if (and_pos != std::string::npos) {
            return eval_expression(trimmed.substr(0, and_pos), obj, raw_line, ignore_case) &&
                   eval_expression(trimmed.substr(and_pos + 2), obj, raw_line, ignore_case);
        }

        if (!trimmed.empty() && trimmed.front() == '!') {
            return !eval_expression(trimmed.substr(1), obj, raw_line, ignore_case);
        }

        std::regex op_re;
        std::smatch match;
        if (GrepSafe::CompileRegex(R"((.+?)\s*(==|!=|>=|<=|>|<|~|!~)\s*(.+))", op_re) && std::regex_match(trimmed, match, op_re)) {
            std::string lhs_str = GrepStringUtils::Trim(match[1].str());
            std::string op      = match[2].str();
            std::string rhs_str = GrepStringUtils::Trim(match[3].str());

            Value lhs = (lhs_str.front() == '.') ? resolve_path(obj, lhs_str) : Value(lhs_str);
            Value rhs = (rhs_str.front() == '.') ? resolve_path(obj, rhs_str) : Value(rhs_str);

            if (rhs_str.size() >= 2 && ((rhs_str.front() == '"' && rhs_str.back() == '"') || (rhs_str.front() == '\'' && rhs_str.back() == '\''))) {
                rhs = Value(rhs_str.substr(1, rhs_str.size() - 2));
            }

            if (op == "==") return lhs.to_string() == rhs.to_string();
            if (op == "!=") return lhs.to_string() != rhs.to_string();
            if (op == "~" || op == "!~") {
                static std::map<std::string, std::regex> regex_cache;
                std::regex::flag_type flags = std::regex::ECMAScript;
                if (ignore_case) flags |= std::regex::icase;
                std::string key = (ignore_case ? "i:" : "s:") + rhs.to_string();
                auto cached = regex_cache.find(key);
                if (cached == regex_cache.end()) {
                    std::regex new_re;
                    if (!GrepSafe::CompileRegex(rhs.to_string(), new_re, flags)) return false;
                    cached = regex_cache.emplace(key, std::move(new_re)).first;
                }
                bool matched = std::regex_search(lhs.to_string(), cached->second);
                return (op == "~") ? matched : !matched;
            }

            double l_num = lhs.to_number();
            double r_num = rhs.to_number();
            if (op == "<")  return l_num < r_num;
            if (op == "<=") return l_num <= r_num;
            if (op == ">")  return l_num > r_num;
            if (op == ">=") return l_num >= r_num;
        }

        if (trimmed.front() == '.') {
            return resolve_path(obj, trimmed).to_bool();
        }

        return !trimmed.empty();
    }
};

// ============================================================================
// 5. CORE SEARCH ENGINE
// ============================================================================
struct MatchResult {
    bool matched = false;
    std::string text_to_display;
    std::vector<std::pair<size_t, size_t>> match_spans;
};

class GrepEngine {
    GrepOptions opts;
    std::vector<std::regex> compiled_regexes;
    ObjectEvaluator object_evaluator;

public:
    explicit GrepEngine(GrepOptions o) : opts(std::move(o)) {
        std::regex::flag_type flags = std::regex::ECMAScript;
        if (opts.ignore_case) flags |= std::regex::icase;

        for (auto& pat : opts.patterns) {
            std::string final_pat = pat;
            if (opts.fixed_strings) {
                std::string escaped;
                for (char ch : final_pat) {
                    if (std::string("\\.^$|()[]{}*+?").find(ch) != std::string::npos) escaped += '\\';
                    escaped += ch;
                }
                final_pat = std::move(escaped);
            }
            if (opts.word_regexp) final_pat = "\\b(?:" + final_pat + ")\\b";
            if (opts.line_regexp) final_pat = "^(?:" + final_pat + ")$";
            std::regex compiled;
            if (GrepSafe::CompileRegex(final_pat, compiled, flags)) {
                compiled_regexes.push_back(std::move(compiled));
            }
        }
    }

    MatchResult evaluate_line(const std::string& line) {
        MatchResult res;
        Value parsed_obj;
        bool is_json = false;

        if (!opts.text_only) {
            std::string trimmed = line;
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
                MiniJsonParser parser(trimmed);
                parsed_obj = parser.parse_value();
                is_json = (parsed_obj.type == Value::OBJECT || parsed_obj.type == Value::ARRAY);
            }
        }

        if (opts.json_mode && !is_json) {
            res.matched = false;
            return res;
        }

        if (!opts.where_expr.empty()) {
            if (!object_evaluator.eval_expression(opts.where_expr, parsed_obj, line, opts.ignore_case)) {
                res.matched = opts.invert_match;
                res.text_to_display = line;
                return res;
            }
            if (compiled_regexes.empty()) {
                res.matched = !opts.invert_match;
                res.text_to_display = resolve_output_text(parsed_obj, line);
                return res;
            }
        }

        std::string target_string = line;
        if (!opts.key_path.empty() && is_json) {
            Value val = object_evaluator.resolve_path(parsed_obj, opts.key_path);
            target_string = val.to_string();
        }

        bool found = false;
        if (compiled_regexes.empty()) {
            found = true;
        } else {
            for (auto& re : compiled_regexes) {
                std::smatch sm;
                std::string::const_iterator search_start(target_string.cbegin());
                while (std::regex_search(search_start, target_string.cend(), sm, re)) {
                    found = true;
                    size_t match_idx = std::distance(target_string.cbegin(), sm[0].first);
                    size_t match_len = sm[0].length();
                    res.match_spans.push_back({match_idx, match_len});
                    if (match_len == 0) {
                        if (search_start == target_string.cend()) break;
                        ++search_start;
                    } else {
                        search_start = sm[0].second;
                    }
                }
            }
        }

        res.matched = opts.invert_match ? !found : found;
        res.text_to_display = resolve_output_text(parsed_obj, line);
        return res;
    }

private:
    std::string resolve_output_text(const Value& obj, const std::string& raw) {
        if (!opts.extract_path.empty() && (obj.type == Value::OBJECT || obj.type == Value::ARRAY)) {
            return object_evaluator.resolve_path(obj, opts.extract_path).to_string();
        }
        return raw;
    }
};

// ============================================================================
// 6. PIPELINE & STREAM PROCESSOR
// ============================================================================
struct ContextLine {
    long long line_num;
    std::string line;
};

class StreamProcessor {
public:
    StreamProcessor(const GrepOptions& options, GrepEngine& grepEngine)
        : opts(options), engine(grepEngine) {}

    void CollectFilesRecursive(const fs::path& dir, std::vector<std::string>& files) const {
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
    }

    long long ProcessStream(std::istream& in, const std::string& filename, bool show_filename) {
        std::string raw_line;
        long long line_num = 0;
        long long match_count = 0;
        unsigned long long byte_offset = 0;
        int after_context_left = 0;
        std::deque<ContextLine> before_buffer;

        while (std::getline(in, raw_line)) {
            const unsigned long long line_offset = byte_offset;
            byte_offset += raw_line.size() + 1;
            if (!opts.text_mode && raw_line.find('\0') != std::string::npos) {
                if (opts.binary_files_without_match) return 0;
                if (!opts.binary_mode) {
                    if (!opts.no_messages && !opts.quiet) std::cerr << "grep: " << filename << ": binary file matches\n";
                    return 1;
                }
            }
            if (!raw_line.empty() && raw_line.back() == '\r') raw_line.pop_back();
            line_num++;

            MatchResult res = engine.evaluate_line(raw_line);

            if (res.matched) {
                match_count++;

                if (opts.files_with_matches) {
                    if (!opts.quiet) std::cout << filename << (opts.null_output ? '\0' : '\n');
                    return match_count;
                }
                if (opts.quiet) return match_count;
                if (opts.count_only) continue;

                while (!before_buffer.empty()) {
                    auto& b = before_buffer.front();
                    if (show_filename) std::cout << COLOR_FILE << filename << COLOR_SEP << "-" << COLOR_RESET;
                    if (opts.line_number) std::cout << COLOR_LINE << b.line_num << COLOR_SEP << "-" << COLOR_RESET;
                    std::cout << b.line << (opts.null_output ? '\0' : '\n');
                    before_buffer.pop_front();
                }

                if (show_filename) std::cout << COLOR_FILE << filename << COLOR_SEP << ":" << COLOR_RESET;
                if (opts.byte_offset) std::cout << line_offset << ":";
                if (opts.line_number) std::cout << COLOR_LINE << line_num << COLOR_SEP << ":" << COLOR_RESET;

                bool offsets_match_display = opts.key_path.empty() && opts.extract_path.empty();
                if (opts.only_matching && offsets_match_display) {
                    for (auto& span : res.match_spans) {
                        std::cout << (opts.color ? COLOR_MATCH : "")
                                  << res.text_to_display.substr(span.first, span.second)
                                  << (opts.color ? COLOR_RESET : "") << (opts.null_output ? '\0' : '\n');
                    }
                } else {
                    if (opts.color && !res.match_spans.empty() && offsets_match_display) {
                        std::sort(res.match_spans.begin(), res.match_spans.end());
                        std::vector<std::pair<size_t, size_t>> merged;
                        for (const auto& span : res.match_spans) {
                            if (span.first >= res.text_to_display.size()) continue;
                            size_t end = (std::min)(res.text_to_display.size(), span.first + span.second);
                            if (merged.empty() || span.first > merged.back().first + merged.back().second) merged.push_back({span.first, end - span.first});
                            else {
                                size_t merged_end = (std::max)(merged.back().first + merged.back().second, end);
                                merged.back().second = merged_end - merged.back().first;
                            }
                        }
                        size_t last = 0;
                        for (const auto& span : merged) {
                            if (span.first > last) std::cout << res.text_to_display.substr(last, span.first - last);
                            std::cout << COLOR_MATCH << res.text_to_display.substr(span.first, span.second) << COLOR_RESET;
                            last = span.first + span.second;
                        }
                        if (last < res.text_to_display.size()) std::cout << res.text_to_display.substr(last);
                        std::cout << (opts.null_output ? '\0' : '\n');
                    } else {
                        std::cout << res.text_to_display << (opts.null_output ? '\0' : '\n');
                    }
                }

                after_context_left = opts.after_context;
                if (opts.max_count > 0 && match_count >= opts.max_count) break;

            } else {
                if (opts.files_without_match || opts.count_only || opts.files_with_matches) continue;

                if (after_context_left > 0) {
                    if (show_filename) std::cout << COLOR_FILE << filename << COLOR_SEP << "-" << COLOR_RESET;
                    if (opts.line_number) std::cout << COLOR_LINE << line_num << COLOR_SEP << "-" << COLOR_RESET;
                    std::cout << raw_line << (opts.null_output ? '\0' : '\n');
                    after_context_left--;
                } else if (opts.before_context > 0) {
                    before_buffer.push_back({line_num, raw_line});
                    if (static_cast<int>(before_buffer.size()) > opts.before_context) {
                        before_buffer.pop_front();
                    }
                }
            }
        }

        if (opts.files_without_match && match_count == 0) {
            if (!opts.quiet) std::cout << filename << (opts.null_output ? '\0' : '\n');
            return 1;
        } else if (opts.count_only) {
            if (show_filename) std::cout << COLOR_FILE << filename << COLOR_SEP << ":" << COLOR_RESET;
            if (!opts.quiet) std::cout << match_count << (opts.null_output ? '\0' : '\n');
        }
        return match_count;
    }

private:
    const GrepOptions& opts;
    GrepEngine& engine;
};

// ============================================================================
// 7. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================
class OptionParser {
public:
    static void DisplayHelp() {
        std::cout << R"(grep(1)             CrossShell for UNIX Reference Manual                 grep(1)

    NAME
        grep - search for patterns in files or input streams

    SYNOPSIS
        grep [OPTIONS] PATTERN [FILE...]
        grep [OPTIONS] -e PATTERN... [FILE...]

    DESCRIPTION
        grep searches for PATTERN in each FILE or standard input. It processes
        both plain text lines and structured objects (JSON Lines / NDJSON),
        permitting deep object navigation (.key, .user.id), numeric and predicate
        filtering (--where), and field extraction (--extract).

    OPTIONS
        -e, --regexp PATTERN
            Specify one or more matching patterns.

        -i, --ignore-case
            Perform case-insensitive matching.

        -E, --extended-regexp
            Interpret PATTERN as an extended regular expression.

        -F, --fixed-strings
            Interpret PATTERN as a fixed string, not a regular expression.

        -P, --perl-regexp
            Request Perl-compatible regular expressions.

        -v, --invert-match
            Invert the sense of matching to select non-matching records.

        -q, --quiet, --silent
            Suppress all normal output; return only exit match status.

        -s, --no-messages
            Suppress error messages about nonexistent or unreadable files.

        -a, --text
            Process binary input as text.

        -b, --byte-offset
            Prefix each output line with its 0-based byte offset in the input.

        -w, --word-regexp
            Force PATTERN to match only whole words.

        -x, --line-regexp
            Force PATTERN to match only exact entire lines.

        -k, --key PATH
            Search PATTERN exclusively inside target object key or array path.

        --where EXPR
            Filter records using expression predicate comparisons.

        --extract PATH
            Output only the target nested object value rather than the whole line.

        -j, --json-only
            Force pure JSON mode; non-JSON lines are ignored.

        -t, --text-only
            Disable object autodetection; treat input purely as plain text.

        -n, --line-number
            Prefix each output line with its 1-based line number.

        -c, --count
            Suppress normal output; print a count of matching lines instead.

        -l, --files-with-matches
            Suppress normal output; print name of each file with matches.

        -L, --files-without-match
            Suppress normal output; print name of each file with no matches.

        -o, --only-matching
            Print only the matched non-empty parts of matching lines.

        -H, --with-filename
            Print the file name for each match.

        -h, --no-filename
            Suppress the file name prefix on output.

        -m, --max-count NUM
            Stop reading a file after NUM matching lines.

        --color[=WHEN]
            Highlight matching strings; WHEN is 'always', 'never', or 'auto'.

        -B, --before-context NUM
            Print NUM lines of leading context before matching lines.

        -A, --after-context NUM
            Print NUM lines of trailing context after matching lines.

        -C, --context NUM
            Print NUM lines of leading and trailing context.

        -r, -R, --recursive
            Read all files under each directory recursively.

        -d, --directories ACTION
            Handle directories with ACTION: read, recurse, or skip.

        -D, --devices ACTION
            Handle devices with ACTION: read or skip.

        -I, --binary-files=without-match
            Treat binary files as having no matches.

        -Z, --null
            Terminate file names and matching output with a NUL byte.

        -U, --binary
            Read binary input without special binary-file handling.

        --registry KEY
            Search registry values emitted as JSON Lines.

        --wmi QUERY[|NAMESPACE]
            Search WMI objects emitted as JSON Lines.

        --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        grep "error" logfile.txt
            Search for literal or regex "error" in logfile.txt.

        grep -n "failed" app.log
            Display matching lines with line numbers.

        grep -E "warn|error" *.log
            Search using extended regular expression pattern.

        grep --where ".status >= 400 && .env == 'prod'" events.jsonl
            Filter JSON Lines using predicate expression.

    CrossShell for UNIX                                                    grep(1)
)";
    }

    static void DisplayVersion() {
        std::cout << "grep version 2.0.0\n"
                  << "Copyright (c) 2026, Roberto J Dohnert\n";
    }

    bool Parse(int argc, char* argv[], GrepOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-?") { DisplayHelp(); exitEarly = true; return true; }
            if (arg == "--version" || arg == "-V") { DisplayVersion(); exitEarly = true; return true; }
            else if ((arg == "-e" || arg == "--regexp") && i + 1 < argc) opts.patterns.push_back(argv[++i]);
            else if ((arg == "-k" || arg == "--key") && i + 1 < argc) opts.key_path = argv[++i];
            else if (arg == "--where" && i + 1 < argc) opts.where_expr = argv[++i];
            else if (arg == "--extract" && i + 1 < argc) opts.extract_path = argv[++i];
            else if (arg == "--ignore-case") opts.ignore_case = true;
            else if (arg == "-E" || arg == "--extended-regexp") { }
            else if (arg == "-F" || arg == "--fixed-strings") opts.fixed_strings = true;
            else if (arg == "-P" || arg == "--perl-regexp") {
                std::cerr << "grep: Perl-compatible regular expressions are not supported\n";
                return false;
            }
            else if (arg == "--invert-match") opts.invert_match = true;
            else if (arg == "-q" || arg == "--quiet" || arg == "--silent") opts.quiet = true;
            else if (arg == "-s" || arg == "--no-messages") opts.no_messages = true;
            else if (arg == "-a" || arg == "--text") opts.text_mode = true;
            else if (arg == "-b" || arg == "--byte-offset") opts.byte_offset = true;
            else if (arg == "--word-regexp") opts.word_regexp = true;
            else if (arg == "--line-regexp") opts.line_regexp = true;
            else if (arg == "--line-number") opts.line_number = true;
            else if (arg == "--count") opts.count_only = true;
            else if (arg == "--files-with-matches") opts.files_with_matches = true;
            else if (arg == "--files-without-match") opts.files_without_match = true;
            else if (arg == "--only-matching") opts.only_matching = true;
            else if (arg == "--with-filename") opts.with_filename = true;
            else if (arg == "--no-filename") opts.no_filename = true;
            else if (arg == "--recursive") opts.recursive = true;
            else if ((arg == "-d" || arg == "--directories") && i + 1 < argc) opts.directory_action = argv[++i];
            else if ((arg == "-D" || arg == "--devices") && i + 1 < argc) opts.devices_action = argv[++i];
            else if (arg == "-I" || arg == "--binary-files=without-match") opts.binary_files_without_match = true;
            else if (arg == "-Z" || arg == "--null") opts.null_output = true;
            else if (arg == "-U" || arg == "--binary") opts.binary_mode = true;
            else if (arg == "-j" || arg == "--json-only") opts.json_mode = true;
            else if ((arg == "--registry" || arg == "--wmi") && i + 1 < argc) opts.object_sources.push_back((arg == "--registry" ? "registry:" : "wmi:") + std::string(argv[++i]));
            else if (arg == "-t" || arg == "--text-only") opts.text_only = true;
            else if (arg == "--color" || arg == "--color=always") { opts.color = true; opts.color_explicit = true; }
            else if (arg == "--color=never") { opts.color = false; opts.color_explicit = true; }
            else if ((arg == "-m" || arg == "--max-count") && i + 1 < argc) {
                int value = 0;
                if (!GrepSafe::ParseInteger(argv[++i], value)) {
                    std::cerr << "grep: invalid numeric value for " << arg << "\n";
                    return false;
                }
                opts.max_count = value;
            }
            else if ((arg == "-B" || arg == "--before-context") && i + 1 < argc) {
                int value = 0;
                if (!GrepSafe::ParseInteger(argv[++i], value)) {
                    std::cerr << "grep: invalid numeric value for " << arg << "\n";
                    return false;
                }
                opts.before_context = value;
            }
            else if ((arg == "-A" || arg == "--after-context") && i + 1 < argc) {
                int value = 0;
                if (!GrepSafe::ParseInteger(argv[++i], value)) {
                    std::cerr << "grep: invalid numeric value for " << arg << "\n";
                    return false;
                }
                opts.after_context = value;
            }
            else if ((arg == "-C" || arg == "--context") && i + 1 < argc) {
                int value = 0;
                if (!GrepSafe::ParseInteger(argv[++i], value)) {
                    std::cerr << "grep: invalid numeric value for " << arg << "\n";
                    return false;
                }
                opts.before_context = value;
                opts.after_context = value;
            } else if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
                // Clustered short flags (e.g. -in, -rn, -v, -ePATTERN)
                bool stop_cluster = false;
                for (size_t j = 1; j < arg.size() && !stop_cluster; ++j) {
                    char c = arg[j];
                    switch (c) {
                        case 'i': opts.ignore_case = true; break;
                        case 'E': break;
                        case 'F': opts.fixed_strings = true; break;
                        case 'q': opts.quiet = true; break;
                        case 's': opts.no_messages = true; break;
                        case 'a': opts.text_mode = true; break;
                        case 'b': opts.byte_offset = true; break;
                        case 'v': opts.invert_match = true; break;
                        case 'w': opts.word_regexp = true; break;
                        case 'x': opts.line_regexp = true; break;
                        case 'n': opts.line_number = true; break;
                        case 'c': opts.count_only = true; break;
                        case 'l': opts.files_with_matches = true; break;
                        case 'L': opts.files_without_match = true; break;
                        case 'o': opts.only_matching = true; break;
                        case 'H': opts.with_filename = true; break;
                        case 'h': opts.no_filename = true; break;
                        case 'r': case 'R': opts.recursive = true; break;
                        case 'j': opts.json_mode = true; break;
                        case 't': opts.text_only = true; break;
                        case 'Z': opts.null_output = true; break;
                        case 'U': opts.binary_mode = true; break;
                        case 'e': {
                            std::string pat = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "");
                            if (!pat.empty()) opts.patterns.push_back(pat);
                            stop_cluster = true;
                            break;
                        }
                        case 'k': {
                            std::string key = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "");
                            if (!key.empty()) opts.key_path = key;
                            stop_cluster = true;
                            break;
                        }
                        case 'm': {
                            std::string val = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "0");
                            int parsed = 0;
                            if (!GrepSafe::ParseInteger(val, parsed)) {
                                std::cerr << "grep: invalid numeric value for -m\n";
                                return false;
                            }
                            opts.max_count = parsed;
                            stop_cluster = true;
                            break;
                        }
                        case 'A': {
                            std::string val = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "0");
                            int parsed = 0;
                            if (!GrepSafe::ParseInteger(val, parsed)) {
                                std::cerr << "grep: invalid numeric value for -A\n";
                                return false;
                            }
                            opts.after_context = parsed;
                            stop_cluster = true;
                            break;
                        }
                        case 'B': {
                            std::string val = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "0");
                            int parsed = 0;
                            if (!GrepSafe::ParseInteger(val, parsed)) {
                                std::cerr << "grep: invalid numeric value for -B\n";
                                return false;
                            }
                            opts.before_context = parsed;
                            stop_cluster = true;
                            break;
                        }
                        case 'C': {
                            std::string val = (j + 1 < arg.size()) ? arg.substr(j + 1) : (i + 1 < argc ? argv[++i] : "0");
                            int parsed = 0;
                            if (!GrepSafe::ParseInteger(val, parsed)) {
                                std::cerr << "grep: invalid numeric value for -C\n";
                                return false;
                            }
                            opts.before_context = parsed;
                            opts.after_context = parsed;
                            stop_cluster = true;
                            break;
                        }
                        default:
                            std::cerr << "grep: unknown option -- '" << c << "'\n";
                            return false;
                    }
                }
            } else if (!arg.empty() && arg[0] != '-') {
                if (opts.patterns.empty() && opts.where_expr.empty()) {
                    opts.patterns.push_back(arg);
                } else {
                    opts.file_paths.push_back(arg);
                }
            }
        }

        if (!opts.color_explicit) {
#ifdef _WIN32
            opts.color = _isatty(_fileno(stdout)) != 0;
#else
            opts.color = isatty(fileno(stdout)) != 0;
#endif
        }

        return true;
    }
};

class GrepApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        GrepOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 2;
        }
        if (exitEarly) {
            return 0;
        }

        if (opts.patterns.empty() && opts.where_expr.empty()) {
            std::cerr << "grep: no pattern or predicate expression provided.\nTry 'grep --help' for more information.\n";
            return 2;
        }

        GrepEngine engine(opts);
        StreamProcessor processor(opts, engine);
        std::vector<std::string> final_files;
        bool has_io_error = false;
        if (opts.recursive && opts.file_paths.empty() && opts.object_sources.empty()) {
            opts.file_paths.push_back(".");
        }
        for (const auto& path_str : opts.file_paths) {
            if (fs::is_directory(path_str)) {
                if (opts.directory_action == "skip") continue;
                if (opts.directory_action == "recurse" || opts.recursive) {
                    processor.CollectFilesRecursive(path_str, final_files);
                } else {
                    if (!opts.no_messages && !opts.quiet) std::cerr << "grep: " << path_str << ": Is a directory\n";
                    has_io_error = true;
                }
            } else {
                final_files.push_back(path_str);
            }
        }
        final_files.insert(final_files.end(), opts.object_sources.begin(), opts.object_sources.end());

        bool show_filename = opts.with_filename || (final_files.size() > 1 && !opts.no_filename);
        long long total_matches = 0;

        if (final_files.empty()) {
            total_matches += processor.ProcessStream(std::cin, "(standard input)", opts.with_filename);
        } else {
            for (const auto& fname : final_files) {
                if (fname.rfind("registry:", 0) == 0 || fname.rfind("wmi:", 0) == 0) {
                    std::string content, error;
                    if (!GrepWindowsObjectLoader::LoadObjectInput(fname, content, error)) {
                        if (!opts.no_messages && !opts.quiet) std::cerr << "grep: " << error << "\n";
                        has_io_error = true;
                        continue;
                    }
                    std::istringstream object_stream(std::move(content));
                    total_matches += processor.ProcessStream(object_stream, fname, show_filename);
                    continue;
                }
                std::ifstream ifs(fname, std::ios::binary);
                if (!ifs.is_open()) {
                    if (!opts.no_messages && !opts.quiet) std::cerr << "grep: " << fname << ": No such file or directory\n";
                    has_io_error = true;
                    continue;
                }
                total_matches += processor.ProcessStream(ifs, fname, show_filename);
            }
        }

        if (has_io_error) {
            return 2;
        }

        return (total_matches > 0) ? 0 : 1;
    }
};

int main(int argc, char* argv[]) {
    GrepApplication app;
    return app.Run(argc, argv);
}