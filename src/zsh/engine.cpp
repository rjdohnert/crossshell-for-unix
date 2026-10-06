#include "engine.hpp"
#include "expansion.hpp"
#include "parser.hpp"
#include "jobs.hpp"
#include "scripting.hpp"

bool g_login_shell = false;
vector<FunctionLocalScope> g_function_local_scopes;

void mark_function_local(const string& name) {
    if (!g_function_local_scopes.empty() && is_valid_env_var_name(name))
        g_function_local_scopes.back().local_names.insert(name);
}

void restore_function_locals(const FunctionLocalScope& scope) {
    for (const auto& name : scope.local_names) {
        auto scalar = scope.vars_before.find(name);
        if (scalar != scope.vars_before.end()) g_env.vars[name] = scalar->second;
        else g_env.vars.erase(name);

        auto indexed = scope.indexed_arrays_before.find(name);
        if (indexed != scope.indexed_arrays_before.end()) g_env.indexed_arrays[name] = indexed->second;
        else g_env.indexed_arrays.erase(name);

        auto assoc = scope.assoc_arrays_before.find(name);
        if (assoc != scope.assoc_arrays_before.end()) g_env.assoc_arrays[name] = assoc->second;
        else g_env.assoc_arrays.erase(name);

        if (scope.integer_vars_before.count(name)) g_env.integer_vars.insert(name);
        else g_env.integer_vars.erase(name);
        if (scope.readonly_vars_before.count(name)) g_env.readonly_vars.insert(name);
        else g_env.readonly_vars.erase(name);
        if (scope.unique_arrays_before.count(name)) g_env.unique_arrays.insert(name);
        else g_env.unique_arrays.erase(name);
    }
    if (scope.local_options) {
        g_env.options = scope.options_before;
    }
}

string canonicalize_option_name(const string& raw) {
    string out;
    out.reserve(raw.size());
    for (char c : raw) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (isalnum(uc)) out += static_cast<char>(tolower(uc));
    }
    return out;
}

void apply_shell_option_token(const string& token, bool enable_default, map<string, bool>& options) {
    string name = canonicalize_option_name(token);
    if (name.empty()) return;

    bool enable = enable_default;
    if (name.size() > 2 && name.rfind("no", 0) == 0) {
        name = name.substr(2);
        enable = !enable_default;
    }
    if (name.empty()) return;

    options[name] = enable;
}

string create_temp_process_subst_path() {
    auto reserve_unique_in_dir = [](const string& raw_dir) -> string {
        error_code ec;
        string dir = normalize_path_to_win(raw_dir);
        fs::create_directories(dir, ec);
        if (ec) return ""; // directory could not be created; caller falls back

        for (int attempt = 0; attempt < 64; ++attempt) {
            unsigned long long nonce =
                (GetTickCount64() ^
                 (static_cast<unsigned long long>(GetCurrentProcessId()) << 16) ^
                 (static_cast<unsigned long long>(GetCurrentThreadId()) << 1) ^
                 static_cast<unsigned long long>(attempt));
            string candidate = dir + "\\zps_" + to_string(GetCurrentProcessId()) + "_" + to_string(nonce) + ".tmp";
            wstring wcandidate = string_to_wstring(candidate);
            HANDLE h = CreateFileW(
                wcandidate.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                CREATE_NEW,
                FILE_ATTRIBUTE_TEMPORARY,
                NULL);
            if (h != INVALID_HANDLE_VALUE) {
                CloseHandle(h);
                return normalize_path_to_win(candidate);
            }
            DWORD gle = GetLastError();
            if (gle != ERROR_FILE_EXISTS && gle != ERROR_ALREADY_EXISTS) break;
        }
        return "";
    };

    char temp_dir[MAX_PATH] = {};
    DWORD n = GetTempPathA(MAX_PATH, temp_dir);
    if (n != 0 && n < MAX_PATH) {
        char temp_file[MAX_PATH] = {};
        if (GetTempFileNameA(temp_dir, "zps", 0, temp_file) != 0) {
            return normalize_path_to_win(string(temp_file));
        }
        string from_temp_dir = reserve_unique_in_dir(string(temp_dir));
        if (!from_temp_dir.empty()) return from_temp_dir;
    }

    string local_tmp = reserve_unique_in_dir("tmp\\zsh_psub");
    if (!local_tmp.empty()) return local_tmp;

    // Last resort: try CWD-relative tmp dir only if it can actually be created.
    {
        error_code ec;
        fs::create_directories("tmp\\zsh_psub", ec);
        if (!ec && fs::is_directory("tmp\\zsh_psub", ec) && !ec) {
            return normalize_path_to_win("tmp\\zsh_psub\\psub_fallback_" + to_string(GetCurrentProcessId()) + "_" + to_string(GetTickCount64()) + ".tmp");
        }
    }
    return ""; // signal failure to caller instead of a bogus path
}

string quote_for_shell_path(const string& p) {
    string q = "\"";
    for (char c : p) {
        if (c == '"') q += "\\\"";
        else q += c;
    }
    q += "\"";
    return q;
}

string win_quote_arg(const string& arg) {
    string r = "\"";
    int bs = 0;
    for (char c : arg) {
        if (c == '\\') { bs++; }
        else if (c == '"') { r += string(2*bs, '\\') + "\\\""; bs = 0; }
        else { r += string(bs, '\\'); bs = 0; r += c; }
    }
    return r + string(2*bs, '\\') + "\"";
}

string cmd_escape_meta(const string& s) {
    static const string meta = "&|<>^()@!";
    string r;
    for (char c : s) {
        if (meta.find(c) != string::npos) r += '^';
        r += c;
    }
    return r;
}

string normalize_path_to_win(string p) {
    if (p.length() >= 2 && p[0] == '/' && isalpha((unsigned char)p[1]) && (p.length() == 2 || p[2] == '/')) {
        string rest = p.length() > 2 ? p.substr(3) : "";
        p = string(1, toupper((unsigned char)p[1])) + ":\\" + rest;
    }
    replace(p.begin(), p.end(), '/', '\\');
    return p;
}

string normalize_path_to_unix(string p) {
    replace(p.begin(), p.end(), '\\', '/');
    return p;
}

bool is_valid_env_var_name(const string& name) {
    if (name.empty()) return false;
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') return false;
    for (char c : name) {
        if (!(isalnum((unsigned char)c) || c == '_')) return false;
    }
    return true;
}

bool contains_dangerous_alias_tokens(const string& value) {
    static const string dangerous = "|&;<>`";
    return value.find_first_of(dangerous) != string::npos ||
           value.find("&&") != string::npos ||
           value.find("||") != string::npos ||
           value.find("$(") != string::npos ||
           value.find("`") != string::npos;
}

wstring string_to_wstring(const string& str) {
    if (str.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    wstring wstr(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], sz);
    return wstr;
}

string current_shell_executable_path() {
    wchar_t executable_path[MAX_PATH] = {};
    DWORD executable_length = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    if (executable_length == 0 || executable_length >= MAX_PATH) return "";
    int utf8_length = WideCharToMultiByte(CP_UTF8, 0, executable_path, static_cast<int>(executable_length),
                                          nullptr, 0, nullptr, nullptr);
    if (utf8_length <= 0) return "";
    string result(static_cast<size_t>(utf8_length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, executable_path, static_cast<int>(executable_length),
                        result.data(), utf8_length, nullptr, nullptr);
    return result;
}

string find_executable_in_path(const string& bin) {
    error_code ec;
    if (fs::exists(bin, ec) && !fs::is_directory(bin, ec)) return bin;
    for (const string& ext : { ".exe", ".com", ".bat", ".cmd" }) {
        if (fs::exists(bin + ext, ec)) return bin + ext;
    }

    string self_path = current_shell_executable_path();
    if (!self_path.empty()) {
        string self_dir = fs::path(self_path).parent_path().string();
        if (!self_dir.empty()) {
            string base = self_dir + "\\" + bin;
            if (fs::exists(base, ec) && !fs::is_directory(base, ec)) return base;
            for (const string& ext : { ".exe", ".com", ".bat", ".cmd" }) {
                if (fs::exists(base + ext, ec)) return base + ext;
            }
        }
    }

    char* path_env = getenv("PATH");
    if (!path_env) return "";

    stringstream ss(path_env);
    string dir;
    while (getline(ss, dir, ';')) {
        string base = normalize_path_to_win(dir) + "\\" + bin;
        if (fs::exists(base, ec) && !fs::is_directory(base, ec)) return base;
        for (const string& ext : { ".exe", ".com", ".bat", ".cmd" }) {
            if (fs::exists(base + ext, ec)) return base + ext;
        }
    }
    return "";
}

bool split_zsh_registry_path(const string& property_path, HKEY& root, wstring& key_path, wstring& value_name) {
    size_t separator = property_path.find('.');
    if (separator == string::npos || separator == 0 || separator + 1 >= property_path.size()) return false;
    string root_name = property_path.substr(0, separator);
    if (root_name == "HKLM" || root_name == "HKEY_LOCAL_MACHINE") root = HKEY_LOCAL_MACHINE;
    else if (root_name == "HKCU" || root_name == "HKEY_CURRENT_USER") root = HKEY_CURRENT_USER;
    else return false;

    string remainder = property_path.substr(separator + 1);
    size_t value_separator = remainder.rfind('.');
    if (value_separator == string::npos || value_separator == 0 || value_separator + 1 >= remainder.size()) return false;
    string raw_key = remainder.substr(0, value_separator);
    value_name = string_to_wstring(remainder.substr(value_separator + 1));
    if (raw_key.find('/') != string::npos) replace(raw_key.begin(), raw_key.end(), '/', '\\');
    else replace(raw_key.begin(), raw_key.end(), '.', '\\');
    key_path = string_to_wstring(raw_key);
    return true;
}

bool read_zsh_registry_property(const string& property_path, string& value, RegistryValueMetadata& metadata) {
    HKEY root = nullptr;
    wstring key_path, value_name;
    if (!split_zsh_registry_path(property_path, root, key_path, value_name)) return false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return false;

    DWORD type = REG_NONE, size = 0;
    LONG status = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &size);
    if (status != ERROR_SUCCESS) { RegCloseKey(key); return false; }
    vector<BYTE> data(size);
    if (size > 0) status = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, data.data(), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) return false;
    data.resize(size);
    metadata.type = type;
    metadata.data = data;

    wstring wide_value;
    if (type == REG_DWORD && data.size() >= sizeof(DWORD)) {
        wide_value = to_wstring(*reinterpret_cast<const DWORD*>(data.data()));
    } else if (type == REG_QWORD && data.size() >= sizeof(ULONGLONG)) {
        wide_value = to_wstring(*reinterpret_cast<const ULONGLONG*>(data.data()));
    } else if (type == REG_BINARY) {
        static const wchar_t hex[] = L"0123456789ABCDEF";
        for (BYTE byte : data) { wide_value += hex[(byte >> 4) & 0x0F]; wide_value += hex[byte & 0x0F]; }
    } else if (type == REG_MULTI_SZ) {
        size_t count = data.size() / sizeof(wchar_t), offset = 0;
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data.data());
        while (offset < count && text[offset] != L'\0') {
            size_t start = offset;
            while (offset < count && text[offset] != L'\0') ++offset;
            if (offset >= count) return false;
            if (!wide_value.empty()) wide_value.push_back(L'\n');
            wide_value.append(text + start, offset - start);
            ++offset;
        }
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        size_t count = data.size() / sizeof(wchar_t);
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data.data());
        while (count > 0 && text[count - 1] == L'\0') --count;
        wide_value.assign(text, count);
    } else {
        return false;
    }

    if (wide_value.empty()) { value.clear(); return true; }
    int length = WideCharToMultiByte(CP_UTF8, 0, wide_value.data(), static_cast<int>(wide_value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return false;
    value.resize(length);
    WideCharToMultiByte(CP_UTF8, 0, wide_value.data(), static_cast<int>(wide_value.size()), value.data(), length, nullptr, nullptr);
    return true;
}

bool write_zsh_registry_property(const string& property_path, const string& value) {
    HKEY root = nullptr;
    wstring key_path, value_name;
    if (!split_zsh_registry_path(property_path, root, key_path, value_name)) return false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;

    DWORD type = REG_SZ, existing_size = 0;
    if (RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &existing_size) != ERROR_SUCCESS) type = REG_SZ;
    vector<BYTE> data;
    if (type == REG_DWORD) {
        char* end = nullptr; unsigned long parsed = strtoul(value.c_str(), &end, 0);
        if (end == value.c_str() || *end != '\0' || parsed > MAXDWORD) { RegCloseKey(key); return false; }
        data.resize(sizeof(DWORD)); DWORD number = static_cast<DWORD>(parsed); memcpy(data.data(), &number, sizeof(number));
    } else if (type == REG_QWORD) {
        char* end = nullptr; unsigned long long parsed = strtoull(value.c_str(), &end, 0);
        if (end == value.c_str() || *end != '\0') { RegCloseKey(key); return false; }
        data.resize(sizeof(ULONGLONG)); ULONGLONG number = static_cast<ULONGLONG>(parsed); memcpy(data.data(), &number, sizeof(number));
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        wstring wide = string_to_wstring(value);
        data.resize((wide.size() + 1) * sizeof(wchar_t));
        memcpy(data.data(), wide.c_str(), data.size());
    } else {
        RegCloseKey(key);
        return false;
    }
    LONG status = RegSetValueExW(key, value_name.c_str(), 0, type, data.data(), static_cast<DWORD>(data.size()));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

ZshEnvironment::ZshEnvironment() {
        char* user_profile = getenv("USERPROFILE");
        if (!user_profile) user_profile = getenv("HOME");
        home_dir = user_profile ? normalize_path_to_win(user_profile) : "C:\\";

        zshrc_path = home_dir + "\\.zshrc";
        history_path = home_dir + "\\.zsh_history";

        options["autocd"] = true;
        options["correct"] = true;
        options["extendedglob"] = true;
        options["interactive"] = true;
        options["promptsubst"] = true;

        vars["ZSH_VERSION"] = "5.9";
        vars["OSTYPE"] = "mswin";
        vars["HOME"] = home_dir;
        vars["PROMPT"] = "[%n@%m] %~ %# ";
        vars["RPROMPT"] = "";
        vars["PROMPT3"] = "?# ";
        vars["TIMEFMT"] = "%E real\t%U user\t%S sys";
        vars["UID"] = "1000";
        vars["EUID"] = "1000";
        vars["GID"] = "1000";
        vars["EGID"] = "1000";
        vars["0"] = "zsh";

        aliases["clear"] = "cls";
        // ls and clear aliases resolved at startup after PATH is checked (see main()).

        try { oldpwd = fs::current_path().string(); } catch (...) { oldpwd = home_dir; }
        vars["OLDPWD"] = normalize_path_to_unix(oldpwd);
        try { vars["PWD"] = normalize_path_to_unix(fs::current_path().string()); }
        catch (...) { vars["PWD"] = vars["OLDPWD"]; }
        SetEnvironmentVariableA("OLDPWD", vars["OLDPWD"].c_str());
        SetEnvironmentVariableA("PWD", vars["PWD"].c_str());
    }

void ZshEnvironment::load_history() {
        ifstream file(history_path);
        if (!file.is_open()) return;
    // load_history already caps to 10000 on read; single-call find for ';'
        string line;
        while (getline(file, line)) {
            if (line.empty()) continue;
            string entry;
            size_t semi = (line[0] == ':') ? line.find(';') : string::npos;
            entry = (semi != string::npos) ? line.substr(semi + 1) : line;
            // Strip non-printable bytes to prevent terminal injection from history file.
            string safe; safe.reserve(entry.size());
            for (unsigned char c : entry) if (c >= 32 || c == '\t') safe += (char)c;
            if (!safe.empty()) history.push_back(safe);
            if (history.size() >= 10000) break; // cap to prevent memory exhaustion
        }
    }

void ZshEnvironment::add_history(const string& cmd) {
        if (cmd.empty()) return;
        if (history.size() >= 10000) history.erase(history.begin()); // keep at most 10000 entries
        history.push_back(cmd);
        ofstream file(history_path, ios::app);
        if (file.is_open()) {
            const time_t now = time(nullptr);
            tm local_tm{};
            char month[32] = {};
            char time_of_day[32] = {};
            if (localtime_s(&local_tm, &now) == 0 &&
                strftime(month, sizeof(month), "%B", &local_tm) != 0 &&
                strftime(time_of_day, sizeof(time_of_day), "%I:%M:%S %p", &local_tm) != 0) {
                file << ": " << month << " " << local_tm.tm_mday << " "
                     << (local_tm.tm_year + 1900) << " " << time_of_day
                     << " unix=" << static_cast<long long>(now) << ":0;"
                     << cmd << "\n";
            }
        }
    }

ZshEnvironment g_env;
