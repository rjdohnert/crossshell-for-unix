// ============================================================================
// UNICODE (UTF-8 <-> UTF-16) TRANSLATION LAYER
// ============================================================================

std::wstring utf8_to_utf16(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
    if (size_needed <= 0) return L"";
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string utf16_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return "";
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::string path_to_utf8(const fs::path& path) {
    auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

// ============================================================================
// WIN32 UTILITIES & SELF-EXE RESOLUTION
// ============================================================================

std::string get_self_exe_path() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    return (len > 0 && len < MAX_PATH) ? utf16_to_utf8(std::wstring(buffer, len)) : "";
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string normalize_path(std::string path) {
    if (path.length() >= 3 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == '/') {
        std::string drive = "";
        drive += static_cast<char>(std::toupper(path[1]));
        drive += ":\\";
        path = drive + path.substr(3);
    }
    for (char &c : path) { if (c == '/') c = '\\'; }
    return path;
}

std::string escape_win32_arg(const std::string& arg) {
    if (arg.empty()) return "\"\"";
    bool needs_quotes = false;
    for (char c : arg) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '"') {
            needs_quotes = true; break;
        }
    }
    if (!needs_quotes) return arg;

    std::string out = "\"";
    size_t backslashes = 0;
    for (char c : arg) {
        if (c == '\\') {
            backslashes++;
        } else if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
            backslashes = 0;
        } else {
            out.append(backslashes, '\\');
            backslashes = 0;
            out.push_back(c);
        }
    }
    out.append(backslashes * 2, '\\');
    out.push_back('"');
    return out;
}

std::string escape_cmd_metachars(const std::string& arg) {
    std::string out;
    for (char c : arg) {
        if (std::string("()<>^&|%!\"").find(c) != std::string::npos) {
            out.push_back('^');
        }
        out.push_back(c);
    }
    return out;
}

std::string resolve_executable_path(const std::string& cmd) {
    std::error_code ec;
    fs::path p(utf8_to_utf16(normalize_path(cmd)));

    wchar_t pathext_buf[32767];
    DWORD pathext_len = GetEnvironmentVariableW(L"PATHEXT", pathext_buf, 32767);
    std::vector<std::wstring> extensions = { L".exe", L".cmd", L".bat", L".com" };
    if (pathext_len > 0) {
        std::wstring pathext_wstr(pathext_buf, pathext_len);
        std::transform(pathext_wstr.begin(), pathext_wstr.end(), pathext_wstr.begin(), ::towlower);
        std::wstringstream ss(pathext_wstr);
        std::wstring ext;
        extensions.clear();
        while (std::getline(ss, ext, L';')) {
            if (!ext.empty()) extensions.push_back(ext);
        }
    }

    auto try_extensions = [&](const fs::path& base) -> std::string {
        if (fs::exists(base, ec) && !fs::is_directory(base, ec)) return utf16_to_utf8(fs::absolute(base, ec).wstring());
        for (const auto& ext : extensions) {
            fs::path candidate = base.wstring() + ext;
            if (fs::exists(candidate, ec) && !fs::is_directory(candidate, ec)) {
                return utf16_to_utf8(fs::absolute(candidate, ec).wstring());
            }
        }
        return "";
    };

    if (p.is_absolute() || cmd.find('\\') != std::string::npos || cmd.find('/') != std::string::npos) {
        std::string res = try_extensions(p);
        if (!res.empty()) return res;
    }

    std::string current_res = try_extensions(fs::current_path() / p);
    if (!current_res.empty()) return current_res;

    wchar_t path_env[32767];
    DWORD len = GetEnvironmentVariableW(L"PATH", path_env, 32767);
    if (len > 0) {
        std::wstring path_wstr(path_env, len);
        std::wstringstream ss(path_wstr);
        std::wstring dir;
        while (std::getline(ss, dir, L';')) {
            if (dir.empty()) continue;
            std::string res = try_extensions(fs::path(dir) / p);
            if (!res.empty()) return res;
        }
    }

    return "";
}

// ============================================================================
// THREAD-SAFE SHELL CONTEXT
// ============================================================================

struct JobInfo {
    int id;
    DWORD pid;
    HANDLE hProcess;
    std::string command;
    bool is_running;
};

struct ShellContext {
    std::map<std::string, std::string> variables;
    std::set<std::string> environment_touched;
    bool pipeline_stage = false;
    int last_return_code = 0;
    DWORD timeout_ms = 60000;
    std::mutex ctx_mutex;
    bool audit_logging_enabled = false;
    std::vector<std::string> history;
    std::vector<JobInfo> jobs;

    std::wstring get_history_file_path() {
        wchar_t profile[32767];
        DWORD len = GetEnvironmentVariableW(L"USERPROFILE", profile, 32767);
        if (len > 0) {
            return std::wstring(profile, len) + L"\\.busybox_history";
        }
        return L".busybox_history";
    }

    void load_history() {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        std::ifstream file(get_history_file_path());
        if (!file) return;

        file.seekg(0, std::ios::end);
        std::streamoff file_size = file.tellg();
        if (file_size < 0 || static_cast<size_t>(file_size) > MAX_HISTORY_FILE_BYTES) {
            return;
        }
        file.seekg(0, std::ios::beg);

        std::string line;
        while (std::getline(file, line)) {
            if (line.size() > MAX_HISTORY_LINE_BYTES) {
                continue;
            }
            if (!line.empty()) {
                history.push_back(line);
                if (history.size() > MAX_HISTORY_ENTRIES) {
                    history.erase(history.begin());
                }
            }
        }
    }

    void save_history() {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        std::ofstream file(get_history_file_path(), std::ios::trunc);
        if (!file) return;
        for (const auto& h : history) {
            file << h << "\n";
        }
    }

    void add_history(const std::string& cmd) {
        if (cmd.empty()) return;
        std::lock_guard<std::mutex> lock(ctx_mutex);
        if (history.empty() || history.back() != cmd) {
            history.push_back(cmd);
            bool rewrite = false;
            if (history.size() > MAX_HISTORY_ENTRIES) {
                history.erase(history.begin());
                rewrite = true;
            }
            if (rewrite) {
                std::ofstream file(get_history_file_path(), std::ios::trunc);
                if (file) {
                    for (const auto& entry : history) file << entry << "\n";
                }
            } else {
                std::ofstream file(get_history_file_path(), std::ios::app);
                if (file) file << cmd << "\n";
            }
        }
    }

    explicit ShellContext(bool load_history_file = true) {
        LPWCH env_strings = GetEnvironmentStringsW();
        if (env_strings) {
            LPWCH var = env_strings;
            while (*var) {
                std::wstring wentry(var);
                std::string entry = utf16_to_utf8(wentry);
                size_t eq = entry.find('=');
                if (eq != std::string::npos && eq > 0) {
                    variables[entry.substr(0, eq)] = entry.substr(eq + 1);
                }
                var += wentry.length() + 1;
            }
            FreeEnvironmentStringsW(env_strings);
        }
        variables["KSH_VERSION"] = "@(#)MIRBSD KSH 2026/07/24";
        variables["SH_VERSION"] = "BusyBox v17.0-PROD";
        if (load_history_file) {
            load_history();
        }
    }

    void set_var(const std::string& name, const std::string& val) {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        variables[name] = val;
        environment_touched.insert(name);
        SetEnvironmentVariableW(utf8_to_utf16(name).c_str(), utf8_to_utf16(val).c_str());
    }

    void unset_var(const std::string& name) {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        variables.erase(name);
        environment_touched.insert(name);
        SetEnvironmentVariableW(utf8_to_utf16(name).c_str(), NULL);
    }

    std::string get_var(const std::string& name) {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        auto it = variables.find(name);
        return (it != variables.end()) ? it->second : "";
    }

    void audit_log(const std::string& action, const std::string& detail) {
        if (!audit_logging_enabled) return;
        std::lock_guard<std::mutex> lock(ctx_mutex);
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm_buf;
        localtime_s(&tm_buf, &now);
        std::clog << "[AUDIT " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") 
                  << " PID:" << GetCurrentProcessId() << "] " 
                  << action << ": " << detail << "\n";
    }
};

bool is_valid_assignment_name(const std::string& name) {
    if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) {
        return false;
    }
    for (size_t i = 1; i < name.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(name[i]);
        if (!(std::isalnum(ch) || ch == '_')) {
            return false;
        }
    }
    return true;
}

void restore_nested_environment(const ShellContext& parent, ShellContext& nested) {
    for (const std::string& name : nested.environment_touched) {
        auto parent_value = parent.variables.find(name);
        if (parent_value != parent.variables.end()) {
            SetEnvironmentVariableW(
                utf8_to_utf16(name).c_str(),
                utf8_to_utf16(parent_value->second).c_str());
        } else {
            SetEnvironmentVariableW(utf8_to_utf16(name).c_str(), nullptr);
        }
    }
}

class ScopedWorkingDirectory {
public:
    ScopedWorkingDirectory() {
        std::error_code ec;
        original_ = fs::current_path(ec);
        active_ = !ec;
    }

    ~ScopedWorkingDirectory() {
        if (active_) {
            std::error_code ec;
            fs::current_path(original_, ec);
        }
    }

private:
    fs::path original_;
    bool active_ = false;
};

// ============================================================================
// RAII HANDLE WRAPPER, JOB OBJECT & SIGNAL MANAGEMENT
// ============================================================================

class ScopedHandle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : h_(h) {}
    ~ScopedHandle() { close(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : h_(other.h_) { other.h_ = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) { close(); h_ = other.h_; other.h_ = INVALID_HANDLE_VALUE; }
        return *this;
    }
    [[nodiscard]] HANDLE get() const { return h_; }
    HANDLE release() { HANDLE tmp = h_; h_ = INVALID_HANDLE_VALUE; return tmp; }
    void close() {
        if (h_ != INVALID_HANDLE_VALUE && h_ != NULL) { CloseHandle(h_); h_ = INVALID_HANDLE_VALUE; }
    }
    [[nodiscard]] bool is_valid() const { return h_ != INVALID_HANDLE_VALUE && h_ != NULL; }
};

static HANDLE g_hJobObject = NULL;
static std::mutex g_proc_mutex;
static std::set<HANDLE> g_active_processes;

void register_active_process(HANDLE h) {
    std::lock_guard<std::mutex> lock(g_proc_mutex);
    g_active_processes.insert(h);
}

void unregister_active_process(HANDLE h) {
    std::lock_guard<std::mutex> lock(g_proc_mutex);
    g_active_processes.erase(h);
}

void init_job_object() {
    g_hJobObject = CreateJobObjectW(NULL, NULL);
    if (g_hJobObject) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
        SetInformationJobObject(g_hJobObject, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
}

void init_vt100_and_binary_mode() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
}

BOOL WINAPI console_signal_handler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        std::lock_guard<std::mutex> lock(g_proc_mutex);
        for (HANDLE h : g_active_processes) {
            TerminateProcess(h, 130);
        }
        std::cout << "\n";
        return TRUE;
    }
    return FALSE;
}

using AppletFunc = std::function<int(const std::vector<std::string>&, ShellContext&)>;
extern std::map<std::string, AppletFunc> applets;

// ============================================================================
// LEXER, TOKENIZER & CHARACTER-LEVEL GLOB EXPANSION ENGINE
// ============================================================================

enum class LexTokenType { Word, Pipe, And, Or, Semi, Ampersand, RedirectIn, RedirectOut, RedirectAppend, LParen, RParen, Newline };

struct Token {
    LexTokenType type;
    std::string value;
};

std::vector<Token> tokenize(const std::string& line) {
    std::vector<Token> tokens;
    std::string current;
    bool in_single = false, in_double = false;
    int cmd_sub_depth = 0;
    bool in_backtick = false;
    int brace_depth = 0;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        if (c == '\'' && !in_double) { in_single = !in_single; current += c; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; current += c; continue; }

        if (c == '\\' && !in_single) {
            current += c;
            if (i + 1 < line.length()) { current += line[++i]; }
            continue;
        }

        if (!in_single && !in_double) {
            if (c == '$' && i + 1 < line.length() && line[i + 1] == '(') {
                cmd_sub_depth++;
                current += "$(";
                i++;
                continue;
            }
            if (c == ')' && cmd_sub_depth > 0) {
                cmd_sub_depth--;
                current += ")";
                continue;
            }
            if (c == '$' && i + 1 < line.length() && line[i + 1] == '{') {
                brace_depth++;
                current += "${";
                i++;
                continue;
            }
            if (c == '}' && brace_depth > 0) {
                brace_depth--;
                current += "}";
                continue;
            }
            if (c == '`') {
                in_backtick = !in_backtick;
                current += "`";
                continue;
            }
        }

        bool inside_sub = (in_single || in_double || cmd_sub_depth > 0 || in_backtick || brace_depth > 0);

        if (!inside_sub) {
            if (i + 1 < line.length()) {
                std::string op2 = line.substr(i, 2);
                if (op2 == "&&" || op2 == "||" || op2 == ">>") {
                    if (!current.empty()) { tokens.push_back({LexTokenType::Word, current}); current.clear(); }
                    if (op2 == "&&") tokens.push_back({LexTokenType::And, "&&"});
                    else if (op2 == "||") tokens.push_back({LexTokenType::Or, "||"});
                    else tokens.push_back({LexTokenType::RedirectAppend, ">>"});
                    i++; continue;
                }
            }
            if (c == '|' || c == '>' || c == '<' || c == ';' || c == '&' || c == '(' || c == ')' || c == '\n') {
                if (!current.empty()) { tokens.push_back({LexTokenType::Word, current}); current.clear(); }
                if (c == '|') tokens.push_back({LexTokenType::Pipe, "|"});
                else if (c == '>') tokens.push_back({LexTokenType::RedirectOut, ">"});
                else if (c == '<') tokens.push_back({LexTokenType::RedirectIn, "<"});
                else if (c == ';') tokens.push_back({LexTokenType::Semi, ";"});
                else if (c == '&') tokens.push_back({LexTokenType::Ampersand, "&"});
                else if (c == '(') tokens.push_back({LexTokenType::LParen, "("});
                else if (c == ')') tokens.push_back({LexTokenType::RParen, ")"});
                else if (c == '\n') tokens.push_back({LexTokenType::Newline, "\n"});
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!current.empty()) { tokens.push_back({LexTokenType::Word, current}); current.clear(); }
                continue;
            }
        }
        current += c;
    }
    if (!current.empty()) tokens.push_back({LexTokenType::Word, current});
    return tokens;
}

std::string capture_cmd_output(const std::string& cmd, ShellContext& ctx);

struct ArgChar {
    char c;
    bool is_quoted;
};

using ArgString = std::vector<ArgChar>;

std::string strip_pattern(const std::string& val, std::string pattern, bool is_prefix, bool longest) {
    if (pattern.empty()) return val;
    std::string regex_pattern;
    for (char c : pattern) {
        if (c == '*') regex_pattern += ".*";
        else if (c == '?') regex_pattern += ".";
        else if (std::string(".+^$()[]{}|\\").find(c) != std::string::npos) regex_pattern += "\\" + std::string(1, c);
        else regex_pattern += c;
    }
    if (is_prefix) {
        regex_pattern = "^(" + regex_pattern + ")";
    } else {
        regex_pattern = "(" + regex_pattern + ")$";
    }
    
    std::regex re;
    try {
        re = std::regex(regex_pattern, std::regex_constants::ECMAScript);
    } catch (...) {
        return val;
    }
    
    if (is_prefix) {
        if (longest) {
            for (size_t len = val.length(); len > 0; --len) {
                std::string sub = val.substr(0, len);
                if (std::regex_match(sub, re)) {
                    return val.substr(len);
                }
            }
        } else {
            for (size_t len = 1; len <= val.length(); ++len) {
                std::string sub = val.substr(0, len);
                if (std::regex_match(sub, re)) {
                    return val.substr(len);
                }
            }
        }
    } else {
        if (longest) {
            for (size_t len = val.length(); len > 0; --len) {
                std::string sub = val.substr(val.length() - len);
                if (std::regex_match(sub, re)) {
                    return val.substr(0, val.length() - len);
                }
            }
        } else {
            for (size_t len = 1; len <= val.length(); ++len) {
                std::string sub = val.substr(val.length() - len);
                if (std::regex_match(sub, re)) {
                    return val.substr(0, val.length() - len);
                }
            }
        }
    }
    return val;
}

ArgString expand_arg_variables(const std::string& input, ShellContext& ctx) {
    ArgString result;
    size_t i = 0, len = input.length();
    bool in_double = false;

    auto append_str = [&](const std::string& s, bool quoted) {
        for (char ch : s) result.push_back({ch, quoted});
    };

    while (i < len) {
        if (input[i] == '\'' && !in_double) {
            i++;
            while (i < len && input[i] != '\'') {
                result.push_back({input[i++], true});
            }
            if (i < len) i++;
            continue;
        }

        if (input[i] == '"') {
            in_double = !in_double;
            i++; continue;
        }

        if (input[i] == '\\' && i + 1 < len) {
            if (in_double) {
                char next_c = input[i + 1];
                if (next_c == '"' || next_c == '\\' || next_c == '$') {
                    result.push_back({next_c, true}); i += 2; continue;
                }
            } else {
                result.push_back({input[i + 1], false}); i += 2; continue;
            }
        }

        if (i + 1 < len && input[i] == '$' && input[i + 1] == '(') {
            size_t depth = 1, start = i + 2, pos = start;
            while (pos < len && depth > 0) {
                if (input[pos] == '(') depth++;
                else if (input[pos] == ')') depth--;
                if (depth > 0) pos++;
            }
            if (depth == 0) {
                append_str(capture_cmd_output(input.substr(start, pos - start), ctx), in_double);
                i = pos + 1; continue;
            }
        }

        if (input[i] == '`') {
            size_t start = i + 1, pos = start;
            while (pos < len && input[pos] != '`') {
                if (input[pos] == '\\' && pos + 1 < len) pos += 2;
                else pos++;
            }
            if (pos < len && input[pos] == '`') {
                std::string inner_cmd;
                for (size_t k = start; k < pos; ++k) {
                    if (input[k] == '\\' && k + 1 < pos && (input[k + 1] == '`' || input[k + 1] == '\\' || input[k + 1] == '$')) {
                        inner_cmd += input[k + 1];
                        k++;
                    } else {
                        inner_cmd += input[k];
                    }
                }
                append_str(capture_cmd_output(inner_cmd, ctx), in_double);
                i = pos + 1;
                continue;
            }
        }

        if (input[i] == '$') {
            i++;
            if (i >= len) { result.push_back({'$', in_double}); break; }

            if (input[i] == '{') {
                size_t close_pos = input.find('}', i);
                if (close_pos != std::string::npos) {
                    std::string var_expr = input.substr(i + 1, close_pos - i - 1);
                    i = close_pos + 1;
                     size_t cd = var_expr.find(":-");
                     size_t ce = var_expr.find(":=");
                     size_t cp = var_expr.find(":+");
                     size_t colon = var_expr.find(':');
                     bool is_slicing = false;
                     if (colon != std::string::npos && colon + 1 < var_expr.length() && 
                         var_expr[colon + 1] != '-' && var_expr[colon + 1] != '=' && var_expr[colon + 1] != '+') {
                         is_slicing = true;
                     }

                     if (is_slicing) {
                         std::string var_name = var_expr.substr(0, colon);
                         std::string slice_expr = var_expr.substr(colon + 1);
                         size_t colon2 = slice_expr.find(':');
                         int offset = 0;
                         int length = -1;
                         try {
                             if (colon2 != std::string::npos) {
                                 offset = std::stoi(slice_expr.substr(0, colon2));
                                 length = std::stoi(slice_expr.substr(colon2 + 1));
                             } else {
                                 offset = std::stoi(slice_expr);
                             }
                         } catch (...) {
                             offset = 0;
                             length = -1;
                         }

                         std::string val = ctx.get_var(var_name);
                         int val_len = static_cast<int>(val.length());
                         if (offset < 0) {
                             offset = val_len + offset;
                         }
                         if (offset < 0) offset = 0;
                         if (offset > val_len) offset = val_len;

                         std::string sliced;
                         if (length == -1) {
                             sliced = val.substr(offset);
                         } else {
                             if (length < 0) {
                                 length = (val_len + length) - offset;
                             }
                             if (length < 0) length = 0;
                             sliced = val.substr(offset, length);
                         }
                         append_str(sliced, in_double);
                     } else if (cd != std::string::npos) {
                         std::string val = ctx.get_var(var_expr.substr(0, cd));
                         append_str(val.empty() ? var_expr.substr(cd + 2) : val, in_double);
                     } else if (ce != std::string::npos) {
                         std::string var_name = var_expr.substr(0, ce);
                         std::string val = ctx.get_var(var_name);
                         if (val.empty()) { val = var_expr.substr(ce + 2); ctx.set_var(var_name, val); }
                         append_str(val, in_double);
                     } else if (cp != std::string::npos) {
                         std::string val = ctx.get_var(var_expr.substr(0, cp));
                         if (!val.empty()) append_str(var_expr.substr(cp + 2), in_double);
                     } else if (!var_expr.empty() && var_expr[0] == '#') {
                         append_str(std::to_string(ctx.get_var(var_expr.substr(1)).length()), in_double);
                     } else if (var_expr.find('#') != std::string::npos && var_expr.find('#') > 0) {
                         size_t hash_pos = var_expr.find('#');
                         bool longest = (hash_pos + 1 < var_expr.length() && var_expr[hash_pos + 1] == '#');
                         std::string var_name = var_expr.substr(0, hash_pos);
                         std::string pattern = var_expr.substr(hash_pos + (longest ? 2 : 1));
                         std::string val = ctx.get_var(var_name);
                         append_str(strip_pattern(val, pattern, true, longest), in_double);
                     } else if (var_expr.find('%') != std::string::npos && var_expr.find('%') > 0) {
                         size_t percent_pos = var_expr.find('%');
                         bool longest = (percent_pos + 1 < var_expr.length() && var_expr[percent_pos + 1] == '%');
                         std::string var_name = var_expr.substr(0, percent_pos);
                         std::string pattern = var_expr.substr(percent_pos + (longest ? 2 : 1));
                         std::string val = ctx.get_var(var_name);
                         append_str(strip_pattern(val, pattern, false, longest), in_double);
                     } else {
                         append_str(ctx.get_var(var_expr), in_double);
                     }
                    continue;
                }
            } else if (input[i] == '?') { append_str(std::to_string(ctx.last_return_code), in_double); i++; continue; }
            else if (input[i] == '$') { append_str(std::to_string(GetCurrentProcessId()), in_double); i++; continue; }
            else if (input[i] == '0') {
                std::string v0 = ctx.get_var("0");
                append_str(v0.empty() ? "busybox" : v0, in_double);
                i++;
                continue;
            }
            else if (input[i] == '#') {
                append_str(ctx.get_var("#"), in_double);
                i++;
                continue;
            }
            else if (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_') {
                size_t start = i;
                while (i < len && (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_')) i++;
                append_str(ctx.get_var(input.substr(start, i - start)), in_double);
                continue;
            }
        }
        result.push_back({input[i++], in_double});
    }
    return result;
}

std::string argstring_to_string(const ArgString& arg) {
    std::string s;
    for (const auto& ac : arg) s += ac.c;
    return s;
}

std::vector<std::string> expand_globs(const std::vector<ArgString>& args) {
    std::vector<std::string> result;
    for (const auto& arg : args) {
        bool has_unquoted_wildcard = false;
        for (const auto& ac : arg) {
            if (!ac.is_quoted && (ac.c == '*' || ac.c == '?')) {
                has_unquoted_wildcard = true; break;
            }
        }

        if (!has_unquoted_wildcard) {
            result.push_back(argstring_to_string(arg));
            continue;
        }

        std::string raw_path = argstring_to_string(arg);
        bool is_recursive = (raw_path.find("**") != std::string::npos);

        fs::path base_dir = ".";
        size_t double_star_pos = raw_path.find("**");
        if (double_star_pos != std::string::npos) {
            size_t last_slash = raw_path.substr(0, double_star_pos).find_last_of("/\\");
            if (last_slash != std::string::npos) {
                base_dir = raw_path.substr(0, last_slash);
                if (base_dir.empty()) base_dir = "/";
            }
        } else {
            fs::path p(utf8_to_utf16(normalize_path(raw_path)));
            base_dir = p.has_parent_path() ? p.parent_path() : fs::path(".");
        }

        std::string regex_pattern;
        for (size_t k = 0; k < arg.size(); ++k) {
            if (!arg[k].is_quoted && arg[k].c == '*') {
                if (k + 1 < arg.size() && !arg[k+1].is_quoted && arg[k+1].c == '*') {
                    if (k + 2 < arg.size() && (arg[k+2].c == '/' || arg[k+2].c == '\\')) {
                        regex_pattern += "(?:.*[/\\\\])?";
                        k += 2;
                    } else {
                        regex_pattern += ".*";
                        k++;
                    }
                } else {
                    regex_pattern += "[^/\\\\]*";
                }
            } else if (!arg[k].is_quoted && arg[k].c == '?') {
                regex_pattern += "[^/\\\\]";
            } else if (std::string(".+^$()[]{}|\\").find(arg[k].c) != std::string::npos) {
                regex_pattern += "\\" + std::string(1, arg[k].c);
            } else if (arg[k].c == '/' || arg[k].c == '\\') {
                regex_pattern += "[/\\\\]";
            } else {
                regex_pattern += arg[k].c;
            }
        }

        std::regex re;
        try {
            re = std::regex(regex_pattern, std::regex_constants::ECMAScript | std::regex_constants::icase | std::regex_constants::optimize);
        } catch (const std::regex_error& e) {
            std::cerr << "busybox: glob regex error: " << e.what() << " for pattern: " << regex_pattern << "\n";
            result.push_back(raw_path);
            continue;
        }

        std::error_code ec;
        std::vector<std::string> matches;
        size_t scanned_entries = 0;
        if (fs::exists(base_dir, ec) && fs::is_directory(base_dir, ec)) {
            if (is_recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(base_dir, ec)) {
                    if (++scanned_entries > MAX_DIRECTORY_SCAN_ENTRIES) break;
                    std::string fname = path_to_utf8(entry.path().filename());
                    if (fname == "." || fname == "..") continue;
                    std::string path_str = path_to_utf8(entry.path());
                    std::string rel_path = path_str;
                    if (rel_path.length() >= 2 && rel_path[0] == '.' && (rel_path[1] == '/' || rel_path[1] == '\\')) {
                        rel_path = rel_path.substr(2);
                    }
                    if (std::regex_match(path_str, re) || std::regex_match(rel_path, re)) {
                        matches.push_back(path_str);
                        if (matches.size() >= MAX_GLOB_MATCHES) break;
                    }
                }
            } else {
                for (const auto& entry : fs::directory_iterator(base_dir, ec)) {
                    if (++scanned_entries > MAX_DIRECTORY_SCAN_ENTRIES) break;
                    std::string fname = path_to_utf8(entry.path().filename());
                    if (fname == "." || fname == "..") continue;
                    std::string path_str = path_to_utf8(entry.path());
                    std::string rel_path = path_str;
                    if (rel_path.length() >= 2 && rel_path[0] == '.' && (rel_path[1] == '/' || rel_path[1] == '\\')) {
                        rel_path = rel_path.substr(2);
                    }
                    if (std::regex_match(fname, re) || std::regex_match(path_str, re) || std::regex_match(rel_path, re)) {
                        matches.push_back(path_str);
                        if (matches.size() >= MAX_GLOB_MATCHES) break;
                    }
                }
            }
        }

        if (!matches.empty()) {
            std::sort(matches.begin(), matches.end());
            for (const auto& m : matches) result.push_back(m);
        } else {
            result.push_back(raw_path);
        }
    }
    return result;
}

