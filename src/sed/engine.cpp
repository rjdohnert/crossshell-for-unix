#include "engine.hpp"

// ============================================================================
// Value implementation
// ============================================================================

std::string Value::to_string() const {
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

double Value::to_number() const {
    if (type == NUMBER) return std::get<double>(data);
    if (type == BOOL) return std::get<bool>(data) ? 1.0 : 0.0;
    if (type == STRING) {
        try { return std::stod(std::get<std::string>(data)); } catch (...) { return 0.0; }
    }
    return 0.0;
}

bool Value::to_bool() const {
    if (type == NIL) return false;
    if (type == BOOL) return std::get<bool>(data);
    if (type == NUMBER) return std::get<double>(data) != 0.0;
    if (type == STRING) return !std::get<std::string>(data).empty() && std::get<std::string>(data) != "0";
    if (type == ARRAY) return !std::get<Array>(data).empty();
    if (type == OBJECT) return !std::get<Object>(data).empty();
    return true;
}

std::string Value::serialize_json() const {
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

Value* Value::get_path_mut(const std::string& key) {
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

const Value* Value::get_path(const std::string& key) const {
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

bool Value::erase_path(const std::string& key) {
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

Value* Value::resolve_ptr(const std::string& path_str, bool create_missing) {
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

bool Value::del_path(const std::string& path_str) {
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
// SedStringUtils implementation
// ============================================================================

std::string SedStringUtils::TranslateReplacement(const std::string& rep) {
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

std::string SedStringUtils::JsonEscape(const std::string& value) {
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
std::string SedStringUtils::WideUtf8(const wchar_t* value) {
    if (!value || *value == L'\0') return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring SedStringUtils::Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 1) return {};
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
    return out;
}

std::string SedStringUtils::VariantJson(const VARIANT& value) {
    VARIANT converted;
    VariantInit(&converted);
    if (FAILED(VariantChangeType(&converted, const_cast<VARIANT*>(&value), 0, VT_BSTR))) return "null";
    std::string out = "\"" + JsonEscape(WideUtf8(converted.bstrVal)) + "\"";
    VariantClear(&converted);
    return out;
}
#endif

// ============================================================================
// SedWindowsObjectLoader implementation
// ============================================================================

#ifdef _WIN32
bool SedWindowsObjectLoader::LoadRegistry(const std::string& spec, std::string& output, std::string& error) {
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

bool SedWindowsObjectLoader::LoadWmi(const std::string& spec, std::string& output, std::string& error) {
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

bool SedWindowsObjectLoader::LoadObjectInput(const std::string& source, std::string& output, std::string& error) {
#ifdef _WIN32
    if (source.rfind("registry:", 0) == 0) return LoadRegistry(source.substr(9), output, error);
    return LoadWmi(source.substr(4), output, error);
#else
    error = "registry and WMI input require Windows"; return false;
#endif
}

// ============================================================================
// Evaluator implementation
// ============================================================================

std::string Evaluator::trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();
    return (end <= start ? std::string() : std::string(start, end));
}

bool Evaluator::eval_condition(const std::string& expr, const Value& obj) const {
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

bool Evaluator::match_address(const Address& addr, long long line_nr, const std::string& line, const Value& obj) const {
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

// ============================================================================
// SedEngine implementation
// ============================================================================

SedEngine::SedEngine(const std::vector<SedCommand>& commands, const SedOptions& options)
    : m_commands(commands), m_options(options) {}

void SedEngine::ProcessStream(std::istream& in, std::ostream& out) const {
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
