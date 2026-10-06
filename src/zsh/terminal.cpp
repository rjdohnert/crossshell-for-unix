#include "terminal.hpp"
#include "engine.hpp"
#include "parser.hpp"
#include "builtins.hpp"
#include "jobs.hpp"
#include "scripting.hpp"

void enable_ansi_support() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

string format_now_header_line() {
    const time_t now = time(nullptr);
    tm local_tm{};
    if (localtime_s(&local_tm, &now) != 0) {
        return "Date/Time unavailable";
    }
    char weekday_month[96] = {};
    char time_12h[32] = {};
    if (strftime(weekday_month, sizeof(weekday_month), "%A %B", &local_tm) == 0) {
        return "Date/Time unavailable";
    }
    if (strftime(time_12h, sizeof(time_12h), "%I:%M:%S %p", &local_tm) == 0) {
        return "Date/Time unavailable";
    }
    ostringstream out;
    out << weekday_month << " " << local_tm.tm_mday << " " << (local_tm.tm_year + 1900)
        << " " << time_12h;
    return out.str();
}

static string render_prompt_template(const string& fmt) {
    string prompt = "";
    for (size_t i = 0; i < fmt.length(); ++i) {
        if (fmt[i] == '%' && i + 1 < fmt.length()) {
            char code = fmt[i + 1];
            if (code == 'F') {
                if (i + 2 < fmt.length() && fmt[i + 2] == '{') {
                    size_t close = fmt.find('}', i + 3);
                    if (close != string::npos) {
                        static const unordered_map<string, string> color_map = {
                            {"black",      "\033[30m"}, {"red",        "\033[31m"},
                            {"green",      "\033[32m"}, {"yellow",     "\033[33m"},
                            {"blue",       "\033[34m"}, {"magenta",    "\033[35m"},
                            {"cyan",       "\033[36m"}, {"white",      "\033[37m"},
                            {"orange",     "\033[38;5;208m"}, {"pink",       "\033[38;5;205m"},
                            {"br_black",   "\033[90m"}, {"br_red",     "\033[91m"},
                            {"br_green",   "\033[92m"}, {"br_yellow",  "\033[93m"},
                            {"br_blue",    "\033[94m"}, {"br_magenta", "\033[95m"},
                            {"br_cyan",    "\033[96m"}, {"br_white",   "\033[97m"},
                        };
                        auto it = color_map.find(fmt.substr(i + 3, close - (i + 3)));
                        if (it != color_map.end()) prompt += it->second;
                        i = close; // loop i++ steps past '}'
                    } else {
                        i++; // no closing brace; skip 'F'
                    }
                } else {
                    i++; // no '{'; skip 'F'
                }
            } else if (code == 'f') { prompt += COLOR_RESET; i++; }
            else if (code == 'n') {
                DWORD l = 0;
                GetUserNameW(nullptr, &l); // l = required size incl. NUL
                if (l > 0) {
                    wstring u(l, L'\0');
                    if (GetUserNameW(&u[0], &l)) {
                        int sz = WideCharToMultiByte(CP_UTF8, 0, u.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (sz > 1) { string s(sz - 1, '\0'); WideCharToMultiByte(CP_UTF8, 0, u.c_str(), -1, &s[0], sz, nullptr, nullptr); prompt += s; }
                    }
                }
                i++;
            }
            else if (code == 'm') {
                DWORD l = 0;
                GetComputerNameW(nullptr, &l); // l = required size incl. NUL
                if (l > 0) {
                    wstring h(l, L'\0');
                    if (GetComputerNameW(&h[0], &l)) {
                        int sz = WideCharToMultiByte(CP_UTF8, 0, h.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (sz > 1) { string s(sz - 1, '\0'); WideCharToMultiByte(CP_UTF8, 0, h.c_str(), -1, &s[0], sz, nullptr, nullptr); prompt += s; }
                    }
                }
                i++;
            }
            else if (code == '~') { try { prompt += normalize_path_to_unix(fs::current_path().string()); } catch (...) { prompt += "?"; } i++; }
            else if (code == '1' && i + 2 < fmt.length() && fmt[i + 2] == '~') {
                // %1~ = trailing folder name only
                try {
                    prompt += fs::current_path().filename().string();
                } catch (...) {
                    prompt += "?";
                }
                i += 2;
            }
            else if (code == 'B') { prompt += "\033[1m"; i++; }  // Bold start
            else if (code == 'b') { prompt += "\033[22m"; i++; } // Bold reset
            else if (code == 'U') { prompt += "\033[4m"; i++; }  // Underline start
            else if (code == 'u') { prompt += "\033[24m"; i++; } // Underline reset
            else if (code == '#') {
                // '#' for elevated/root, '%' for normal user
                bool elevated = false;
                HANDLE tok = NULL;
                if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
                    TOKEN_ELEVATION te{}; DWORD sz = 0;
                    if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz))
                        elevated = te.TokenIsElevated != 0;
                    CloseHandle(tok);
                }
                prompt += elevated ? "#" : "%"; i++;
            }
            else if (code == '?') {
                prompt += to_string(g_env.last_exit_code);
                i++;
            }
        } else prompt += fmt[i];
    }
    return prompt;
}

string render_prompt() {
    if (!g_env.prompt_dirty) return g_env.prompt_cache;
    string prompt = render_prompt_template(g_env.vars["PROMPT"]);
    g_env.prompt_cache = prompt;
    g_env.prompt_dirty  = false;
    return prompt;
}

string render_rprompt() {
    auto it = g_env.vars.find("RPROMPT");
    if (it == g_env.vars.end() || it->second.empty()) return "";
    return render_prompt_template(it->second);
}

// ============================================================================
// TAB COMPLETION ENGINE
// ============================================================================
static string str_lower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)tolower(c); });
    return s;
}

vector<string> get_pathext_list() {
    vector<string> exts;
    char* env_pathext = getenv("PATHEXT");
    string raw = env_pathext ? env_pathext : ".COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC;.PY;.PS1";
    stringstream ss(raw);
    string item;
    while (getline(ss, item, ';')) {
        while (!item.empty() && isspace((unsigned char)item.front())) item.erase(item.begin());
        while (!item.empty() && isspace((unsigned char)item.back())) item.pop_back();
        if (!item.empty()) {
            if (item.front() != '.') item = "." + item;
            exts.push_back(str_lower(item));
        }
    }
    if (exts.empty()) {
        exts = {".com", ".exe", ".bat", ".cmd", ".vbs", ".vbe", ".js", ".jse", ".wsf", ".wsh", ".msc", ".py", ".ps1"};
    }
    return exts;
}

int match_fuzzy_score(const string& query, const string& candidate) {
    if (query.empty()) return 100;
    string q = str_lower(query);
    string c = str_lower(candidate);
    if (c == q) return 1000;
    if (c.rfind(q, 0) == 0) return 800 - static_cast<int>(c.size() - q.size());

    // Word boundary prefix match (e.g. c-e or c_e matching cmd-extended)
    size_t qi = 0;
    for (size_t ci = 0; ci < c.size() && qi < q.size(); ++ci) {
        if (ci == 0 || c[ci - 1] == '-' || c[ci - 1] == '_' || c[ci - 1] == '.') {
            if (c[ci] == q[qi]) qi++;
        }
    }
    if (qi == q.size()) return 600 - static_cast<int>(c.size() - q.size());

    // Substring match
    size_t sub_pos = c.find(q);
    if (sub_pos != string::npos) {
        return 400 - static_cast<int>(sub_pos * 10 + (c.size() - q.size()));
    }

    // Fuzzy subsequence match
    qi = 0;
    int contiguous_bonus = 0;
    int last_match_idx = -2;
    for (size_t ci = 0; ci < c.size() && qi < q.size(); ++ci) {
        if (c[ci] == q[qi]) {
            if (static_cast<int>(ci) == last_match_idx + 1) contiguous_bonus += 15;
            if (ci == 0 || c[ci - 1] == '-' || c[ci - 1] == '_' || c[ci - 1] == '.') contiguous_bonus += 20;
            last_match_idx = static_cast<int>(ci);
            qi++;
        }
    }
    if (qi == q.size()) {
        return 200 + contiguous_bonus - static_cast<int>(c.size());
    }

    return 0; // No match
}

struct CompState {
    vector<string> candidates;
    size_t         idx = 0;
    string         stem;
    size_t         stem_pos = 0;
    bool           active = false;
    int            rendered_rows = 0;
    string         original_buf;
    size_t         original_cursor = 0;
};
static CompState g_comp;

static void clear_completion_menu() {
    if (g_comp.rendered_rows > 0) {
        string out;
        for (int r = 0; r < g_comp.rendered_rows; ++r) {
            out += "\n\033[2K\r";
        }
        out += "\033[" + to_string(g_comp.rendered_rows) + "A\r";
        fwrite(out.data(), 1, out.size(), stdout);
        fflush(stdout);
        g_comp.rendered_rows = 0;
    }
}

static void reset_comp() {
    clear_completion_menu();
    g_comp.active = false;
    g_comp.candidates.clear();
    g_comp.idx = 0;
    g_comp.rendered_rows = 0;
    g_comp.original_buf.clear();
    g_comp.original_cursor = 0;
}

// Returns the token being completed and sets stem_start to its position in buf.
static string completion_stem(const string& buf, size_t cursor, size_t& stem_start) {
    size_t i = cursor;
    while (i > 0 && !isspace((unsigned char)buf[i-1])) --i;
    stem_start = i;
    return buf.substr(i, cursor - i);
}



// Longest common prefix (case-preserving from first candidate).
static string common_prefix(const vector<string>& v) {
    if (v.empty()) return "";
    string p = v[0];
    for (size_t i = 1; i < v.size(); ++i) {
        size_t j = 0;
        while (j < p.size() && j < v[i].size() && tolower((unsigned char)p[j]) == tolower((unsigned char)v[i][j])) ++j;
        p = p.substr(0, j);
    }
    return p;
}

// Collect filesystem completions for a path prefix with fuzzy & substring matching.
static vector<string> complete_path(const string& prefix) {
    struct MatchItem {
        string full_path;
        int score;
    };
    vector<MatchItem> matches;
    string dir_part, file_part;
    size_t sep = prefix.find_last_of("/\\");
    if (sep == string::npos) { dir_part = "."; file_part = prefix; }
    else { dir_part = prefix.substr(0, sep + 1); file_part = prefix.substr(sep + 1); }

    string win_dir = normalize_path_to_win(g_env.expand_vars(dir_part == "." ? "." : dir_part));
    error_code ec;
    if (!fs::is_directory(win_dir, ec)) return {};

    for (const auto& e : fs::directory_iterator(win_dir, ec)) {
        string name = e.path().filename().string();
        int score = match_fuzzy_score(file_part, name);
        if (score > 0) {
            string full = (dir_part == "." ? "" : dir_part) + name;
            if (e.is_directory(ec)) full += "/";
            matches.push_back({full, score});
        }
    }

    sort(matches.begin(), matches.end(), [](const MatchItem& a, const MatchItem& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.full_path.size() != b.full_path.size()) return a.full_path.size() < b.full_path.size();
        return a.full_path < b.full_path;
    });

    vector<string> out;
    for (const auto& m : matches) out.push_back(m.full_path);
    return out;
}

// Collect command completions (builtins + aliases + functions + PATH via %PATHEXT%) with fuzzy matching.
vector<string> complete_command(const string& prefix) {
    struct MatchItem {
        string name;
        int score;
    };
    vector<MatchItem> matches;
    set<string> seen;

    auto check_and_add = [&](const string& name) {
        if (name.empty()) return;
        string key = str_lower(name);
        if (seen.count(key)) return;
        seen.insert(key);
        int score = match_fuzzy_score(prefix, name);
        if (score > 0) {
            matches.push_back({name, score});
        }
    };

    // Builtins
    for (const auto& b : zsh_builtin_command_names()) {
        check_and_add(b);
    }
    // Aliases
    for (const auto& [k, v] : g_env.aliases) {
        check_and_add(k);
    }
    // Functions
    for (const auto& [k, v] : g_env.functions) {
        check_and_add(k);
    }

    // Executables in PATH with automatic PATHEXT resolution and extension stripping
    vector<string> pathext = get_pathext_list();
    char* path_env = getenv("PATH");
    if (path_env) {
        stringstream ss(path_env);
        string dir;
        while (getline(ss, dir, ';')) {
            while (!dir.empty() && isspace((unsigned char)dir.front())) dir.erase(dir.begin());
            while (!dir.empty() && isspace((unsigned char)dir.back())) dir.pop_back();
            if (dir.empty()) continue;
            error_code ec;
            if (!fs::is_directory(dir, ec)) continue;
            for (const auto& e : fs::directory_iterator(dir, ec)) {
                if (!e.is_regular_file(ec)) continue;
                string filename = e.path().filename().string();
                string ext = str_lower(e.path().extension().string());
                bool is_executable = false;
                for (const auto& pe : pathext) {
                    if (ext == pe) {
                        is_executable = true;
                        break;
                    }
                }
                if (is_executable) {
                    string stem = e.path().stem().string();
                    check_and_add(stem);
                    if (prefix.find('.') != string::npos) {
                        check_and_add(filename);
                    }
                }
            }
        }
    }

    sort(matches.begin(), matches.end(), [](const MatchItem& a, const MatchItem& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.name.size() != b.name.size()) return a.name.size() < b.name.size();
        return a.name < b.name;
    });

    vector<string> out;
    for (const auto& m : matches) out.push_back(m.name);
    return out;
}

vector<string> complete_registered_command(const string& command, const string& prefix) {
    auto definition = g_env.completion_definitions.find(command);
    if (definition == g_env.completion_definitions.end()) return {};
    vector<string> saved_reply = g_env.indexed_arrays["reply"];
    g_env.indexed_arrays["reply"].clear();
    g_env.vars["PREFIX"] = prefix;
    dispatch_command({definition->second});
    struct MatchItem {
        string val;
        int score;
    };
    vector<MatchItem> matches;
    set<string> seen;
    for (const auto& candidate : g_env.indexed_arrays["reply"]) {
        if (seen.count(candidate)) continue;
        seen.insert(candidate);
        int score = match_fuzzy_score(prefix, candidate);
        if (score > 0) matches.push_back({candidate, score});
    }
    g_env.indexed_arrays["reply"] = std::move(saved_reply);
    sort(matches.begin(), matches.end(), [](const MatchItem& a, const MatchItem& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.val < b.val;
    });
    vector<string> out;
    for (const auto& m : matches) out.push_back(m.val);
    return out;
}

bool execute_bound_widget(int key_code, string& buffer, size_t& cursor) {
    string literal_key(1, static_cast<char>(key_code));
    string caret_key;
    if (key_code >= 1 && key_code <= 26) caret_key = string("^") + static_cast<char>('A' + key_code - 1);
    const string keymaps[] = {"main", "emacs", "emacs-standard"};
    string widget_name;
    for (const auto& keymap : keymaps) {
        auto map_it = g_env.keymaps.find(keymap);
        if (map_it == g_env.keymaps.end()) continue;
        auto binding = map_it->second.find(literal_key);
        if (binding == map_it->second.end() && !caret_key.empty()) binding = map_it->second.find(caret_key);
        if (binding != map_it->second.end()) { widget_name = binding->second; break; }
    }
    auto widget = g_env.widgets.find(widget_name);
    if (widget_name.empty() || widget == g_env.widgets.end()) return false;
    g_env.vars["BUFFER"] = buffer;
    g_env.vars["CURSOR"] = to_string(cursor);
    dispatch_command({widget->second});
    buffer = g_env.vars["BUFFER"];
    try { cursor = min(buffer.size(), static_cast<size_t>(stoull(g_env.vars["CURSOR"]))); }
    catch (...) { cursor = buffer.size(); }
    return true;
}

// Returns the visible display column count of a string (stripping ANSI escapes & handling UTF-8 multi-byte glyphs).
size_t visible_length(const string& s) {
    size_t len = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i+1] == '[') {
            i += 2;
            while (i < s.size() && s[i] != 'm' && s[i] != 'K' && s[i] != 'G' && s[i] != 'H') ++i;
        } else {
            unsigned char uc = static_cast<unsigned char>(s[i]);
            // In UTF-8, continuation bytes match 10xxxxxx (0x80 to 0xBF). Skip them so each multi-byte glyph counts as 1 display column.
            if ((uc & 0xC0) != 0x80) {
                ++len;
            }
        }
    }
    return len;
}

void repaint_line(const string& buf, size_t cursor_pos) {
    string rendered = render_prompt();
    string rprompt = render_rprompt();

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    int console_width = 0;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        console_width = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
        if (console_width <= 0) console_width = csbi.dwSize.X;
    }

    // Build entire output into one buffer for a single atomic write — eliminates flicker.
    string out;
    out.reserve(rendered.size() + buf.size() + 64);
    out += "\r\033[K";
    out += rendered;
    bool first_word = true;
    size_t pos = 0;
    while (pos <= buf.size()) {
        size_t ws_start = pos;
        while (pos < buf.size() && !isspace((unsigned char)buf[pos])) ++pos;
        if (pos > ws_start) {
            string word = buf.substr(ws_start, pos - ws_start);
            if (first_word) {
                out += (g_env.aliases.count(word) || g_env.functions.count(word)) ? COLOR_BR_GREEN : COLOR_BR_BLUE;
                first_word = false;
            } else if (!word.empty() && word[0] == '-') out += COLOR_CYAN;
            else out += COLOR_WHITE;
            out += word;
        }
        while (pos < buf.size() && isspace((unsigned char)buf[pos])) out += buf[pos++];
        if (pos >= buf.size()) break;
    }
    out += COLOR_RESET;

    size_t prompt_and_input_len = visible_length(rendered) + buf.size();
    size_t rlen = visible_length(rprompt);
    if (rlen > 0 && console_width > (int)(prompt_and_input_len + rlen + 2)) {
        int start_col = console_width - (int)rlen + 1;
        if (start_col < 1) start_col = 1;
        out += "\033[" + to_string(start_col) + "G" + rprompt;
    }

    out += "\033[" + to_string(visible_length(rendered) + cursor_pos + 1) + "G";
    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);
}

static void render_completion_menu_grid(const string& buf, size_t cursor_pos) {
    if (!g_comp.active || g_comp.candidates.empty()) {
        clear_completion_menu();
        repaint_line(buf, cursor_pos);
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    int console_width = 80;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        console_width = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
        if (console_width <= 0) console_width = csbi.dwSize.X;
    }
    if (console_width < 20) console_width = 80;

    size_t max_len = 0;
    for (const auto& c : g_comp.candidates) {
        if (c.size() > max_len) max_len = c.size();
    }
    size_t col_width = min(static_cast<size_t>(console_width), max_len + 3);
    size_t num_cols = max(size_t(1), static_cast<size_t>(console_width) / col_width);
    size_t total_items = g_comp.candidates.size();
    size_t total_rows = (total_items + num_cols - 1) / num_cols;

    const size_t max_visible_rows = 5;
    size_t sel_row = g_comp.idx / num_cols;
    size_t start_row = 0;
    if (sel_row >= max_visible_rows) {
        start_row = sel_row - max_visible_rows + 1;
    }
    size_t end_row = min(total_rows, start_row + max_visible_rows);

    // Repaint the prompt and command line first
    repaint_line(buf, cursor_pos);

    // Build the grid menu output
    string out;
    int rows_drawn = 0;

    for (size_t r = start_row; r < end_row; ++r) {
        out += "\n\033[2K\r";
        for (size_t col = 0; col < num_cols; ++col) {
            size_t item_idx = r * num_cols + col;
            if (item_idx >= total_items) break;
            const string& item = g_comp.candidates[item_idx];
            bool is_selected = (item_idx == g_comp.idx);

            string pad_str = item;
            if (pad_str.size() < col_width) {
                pad_str.append(col_width - pad_str.size(), ' ');
            } else if (pad_str.size() > col_width) {
                pad_str = pad_str.substr(0, col_width);
            }

            if (is_selected) {
                out += "\033[7m\033[1;36m" + pad_str + "\033[0m";
            } else {
                out += "\033[36m" + pad_str + "\033[0m";
            }
        }
        rows_drawn++;
    }

    if (total_rows > max_visible_rows) {
        out += "\n\033[2K\r\033[90m-- (" + to_string(g_comp.idx + 1) + "/" + to_string(total_items) + ") --\033[0m";
        rows_drawn++;
    }

    // Clear previously rendered lines if fewer rows are rendered now
    if (g_comp.rendered_rows > rows_drawn) {
        for (int extra = rows_drawn; extra < g_comp.rendered_rows; ++extra) {
            out += "\n\033[2K\r";
            rows_drawn++;
        }
    }

    // Move cursor back up to the prompt line
    if (rows_drawn > 0) {
        out += "\033[" + to_string(rows_drawn) + "A\r";
    }

    // Reposition cursor horizontally on prompt line
    string rendered = render_prompt();
    out += "\033[" + to_string(visible_length(rendered) + cursor_pos + 1) + "G";

    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);

    g_comp.rendered_rows = rows_drawn;
}

// ============================================================================
// TYPO CORRECTION ENGINE (setopt CORRECT)
// ============================================================================
int damerau_levenshtein_distance(const string& s1_in, const string& s2_in) {
    string a = str_lower(s1_in);
    string b = str_lower(s2_in);
    size_t len_a = a.size();
    size_t len_b = b.size();
    if (len_a == 0) return static_cast<int>(len_b);
    if (len_b == 0) return static_cast<int>(len_a);

    vector<vector<int>> d(len_a + 1, vector<int>(len_b + 1, 0));
    for (size_t i = 0; i <= len_a; ++i) d[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len_b; ++j) d[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= len_a; ++i) {
        for (size_t j = 1; j <= len_b; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int deletion = d[i - 1][j] + 1;
            int insertion = d[i][j - 1] + 1;
            int substitution = d[i - 1][j - 1] + cost;
            int min_val = min({deletion, insertion, substitution});

            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                min_val = min(min_val, d[i - 2][j - 2] + 1); // transposition
            }
            d[i][j] = min_val;
        }
    }
    return d[len_a][len_b];
}

static bool is_valid_command_name(const string& cmd) {
    if (cmd.empty()) return true;
    if (is_zsh_builtin_command(cmd)) return true;
    if (g_env.aliases.count(cmd)) return true;
    if (g_env.functions.count(cmd)) return true;
    if (!find_executable_in_path(cmd).empty()) return true;

    // Check autocd: drive letter or directory
    error_code ec;
    string win_cmd = normalize_path_to_win(cmd);
    bool is_drive_letter = (cmd.size() == 2 && isalpha((unsigned char)cmd[0]) && cmd[1] == ':');
    if (g_env.options.count("autocd") && g_env.options.at("autocd") &&
        (is_drive_letter || fs::is_directory(win_cmd, ec))) {
        return true;
    }

    // Check direct path to executable
    if (cmd.find_first_of("/\\") != string::npos) {
        if (fs::exists(win_cmd, ec) && !fs::is_directory(win_cmd, ec)) return true;
        for (const string& ext : { ".exe", ".com", ".bat", ".cmd" }) {
            if (fs::exists(win_cmd + ext, ec)) return true;
        }
    }

    return false;
}

string find_typo_correction(const string& token) {
    if (token.size() <= 1) return "";
    if (token.find_first_of("/\\") != string::npos) return "";
    if (is_valid_command_name(token)) return "";

    int max_distance = (token.size() <= 4) ? 1 : 2;

    struct TypoMatch {
        string name;
        int distance;
        bool is_transposition;
        bool is_builtin_or_alias;
        int prefix_len;
    };
    vector<TypoMatch> matches;
    set<string> seen;

    string token_lower = str_lower(token);

    auto check_candidate = [&](const string& name, bool is_builtin_alias) {
        if (name.empty() || name == token) return;
        string low = str_lower(name);
        if (seen.count(low)) return;
        seen.insert(low);

        int dist = damerau_levenshtein_distance(token_lower, low);
        if (dist <= max_distance) {
            bool is_trans = (dist == 1 && token_lower.size() == low.size() &&
                             is_permutation(token_lower.begin(), token_lower.end(), low.begin()));
            int prefix_len = 0;
            while (prefix_len < (int)token_lower.size() && prefix_len < (int)low.size() &&
                   token_lower[prefix_len] == low[prefix_len]) {
                prefix_len++;
            }
            matches.push_back({name, dist, is_trans, is_builtin_alias, prefix_len});
        }
    };

    // 1. Builtins
    for (const auto& b : zsh_builtin_command_names()) check_candidate(b, true);
    // 2. Aliases
    for (const auto& [k, v] : g_env.aliases) check_candidate(k, true);
    // 3. Functions
    for (const auto& [k, v] : g_env.functions) check_candidate(k, true);
    // 4. PATH executables & companion directory
    vector<string> pathext = get_pathext_list();
    vector<string> search_dirs;
    string self_path = current_shell_executable_path();
    if (!self_path.empty()) {
        string self_dir = fs::path(self_path).parent_path().string();
        if (!self_dir.empty()) search_dirs.push_back(self_dir);
    }
    char* path_env = getenv("PATH");
    if (path_env) {
        stringstream ss(path_env);
        string dir;
        while (getline(ss, dir, ';')) {
            while (!dir.empty() && isspace((unsigned char)dir.front())) dir.erase(dir.begin());
            while (!dir.empty() && isspace((unsigned char)dir.back())) dir.pop_back();
            if (!dir.empty()) search_dirs.push_back(normalize_path_to_win(dir));
        }
    }
    for (const auto& dir : search_dirs) {
        error_code ec;
        if (!fs::is_directory(dir, ec)) continue;
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (!e.is_regular_file(ec)) continue;
            string ext = str_lower(e.path().extension().string());
            bool is_exec = false;
            for (const auto& pe : pathext) {
                if (ext == pe) { is_exec = true; break; }
            }
            if (is_exec) {
                check_candidate(e.path().stem().string(), false);
            }
        }
    }

    if (matches.empty()) return "";

    sort(matches.begin(), matches.end(), [](const TypoMatch& a, const TypoMatch& b) {
        if (a.distance != b.distance) return a.distance < b.distance;
        if (a.is_transposition != b.is_transposition) return a.is_transposition > b.is_transposition;
        if (a.is_builtin_or_alias != b.is_builtin_or_alias) return a.is_builtin_or_alias > b.is_builtin_or_alias;
        if (a.prefix_len != b.prefix_len) return a.prefix_len > b.prefix_len;
        return a.name.size() < b.name.size();
    });

    return matches[0].name;
}

string replace_first_command_token(const string& line, const string& old_tok, const string& new_tok) {
    size_t start = 0;
    while (start < line.size() && isspace((unsigned char)line[start])) ++start;
    if (start < line.size() && line.compare(start, old_tok.size(), old_tok) == 0) {
        size_t after = start + old_tok.size();
        if (after == line.size() || isspace((unsigned char)line[after])) {
            return line.substr(0, start) + new_tok + line.substr(after);
        }
    }
    return new_tok + (line.size() > old_tok.size() ? line.substr(old_tok.size()) : "");
}

static bool apply_interactive_typo_correction(const string& line, string& action_line, string& pending_edit_line) {
    action_line = line;
    pending_edit_line.clear();

    if (!g_env.options.count("correct") || !g_env.options.at("correct")) return true;

    vector<string> words = tokenize_words(line);
    if (words.empty()) return true;

    // Skip assignments VAR=val
    size_t cmd_idx = 0;
    while (cmd_idx < words.size() && words[cmd_idx].find('=') != string::npos) {
        cmd_idx++;
    }
    if (cmd_idx >= words.size()) return true;

    const string& cmd_tok = words[cmd_idx];

    // Check for nocorrect or keywords
    if (cmd_tok == "nocorrect" || cmd_tok == "noglob" || cmd_tok == "builtin" || cmd_tok == "command" ||
        cmd_tok == "if" || cmd_tok == "for" || cmd_tok == "while" || cmd_tok == "until" ||
        cmd_tok == "case" || cmd_tok == "repeat" || cmd_tok == "select" || cmd_tok == "function" ||
        cmd_tok == "time" || cmd_tok == "exec") {
        return true;
    }

    if (cmd_tok.size() <= 1 || cmd_tok.find_first_of("/\\") != string::npos) return true;
    if (is_valid_command_name(cmd_tok)) return true;

    string suggestion = find_typo_correction(cmd_tok);
    if (suggestion.empty() || suggestion == cmd_tok) return true;

    cout << "zsh: correct '" << cmd_tok << "' to '" << suggestion << "' [nyae]? ";
    cout.flush();

    int c = _getch();
    if (c == 'y' || c == 'Y' || c == 13 || c == 10) {
        cout << "y\n";
        action_line = replace_first_command_token(line, cmd_tok, suggestion);
        return true;
    } else if (c == 'n' || c == 'N' || c == ' ') {
        cout << "n\n";
        action_line = line;
        return true;
    } else if (c == 'a' || c == 'A' || c == 27 || c == 3) {
        cout << "a\n";
        g_env.last_exit_code = 1;
        return false;
    } else if (c == 'e' || c == 'E') {
        cout << "e\n";
        pending_edit_line = replace_first_command_token(line, cmd_tok, suggestion);
        return false;
    } else {
        cout << "n\n";
        action_line = line;
        return true;
    }
}

string read_line_interactive(const string& initial_buffer) {
    string buf = initial_buffer; size_t cursor_pos = buf.size(); int hist_idx = (int)g_env.history.size();
    bool reverse_search_active = false;
    string reverse_search_query;
    int reverse_search_pos = (int)g_env.history.size() - 1;
    reset_comp();
    if (!buf.empty()) {
        repaint_line(buf, cursor_pos);
    }

    while (true) {
        check_window_resize_event();
        process_pending_traps();

        if (!_kbhit()) {
            Sleep(10);
            continue;
        }

        int c = _getch();
        if (execute_bound_widget(c, buf, cursor_pos)) {
            reset_comp();
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c != 18) {
            reverse_search_active = false;
            reverse_search_query.clear();
            reverse_search_pos = (int)g_env.history.size() - 1;
        }
        if (c == 26) { // Ctrl+Z: SIGTSTP emulation
            reset_comp();
            cout << "^Z\n";

            if (suspend_active_foreground_processes()) {
                cout << "suspended active foreground process(es)\n";
            }

            g_sigtstp_pending.store(true);
            process_pending_traps();
            buf.clear();
            cursor_pos = 0;
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 3) { // Ctrl+C: Cancel current line
            reset_comp();
            cout << "^C\n";
            buf.clear();
            cursor_pos = 0;
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 1) { // Ctrl+A: Jump to start of line
            reset_comp();
            cursor_pos = 0;
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 5) { // Ctrl+E: Jump to end of line
            reset_comp();
            cursor_pos = buf.size();
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 12) { // Ctrl+L: Clear screen & repaint prompt without losing buffer
            reset_comp();
            system("cls");
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 21) { // Ctrl+U: Delete line from start to cursor
            reset_comp();
            buf.erase(0, cursor_pos);
            cursor_pos = 0;
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 11) { // Ctrl+K: Delete line from cursor to end
            reset_comp();
            buf.erase(cursor_pos);
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 23) { // Ctrl+W: Erase word backwards
            reset_comp();
            while (cursor_pos > 0 && isspace((unsigned char)buf[cursor_pos - 1])) {
                buf.erase(--cursor_pos, 1);
            }
            while (cursor_pos > 0 && !isspace((unsigned char)buf[cursor_pos - 1])) {
                buf.erase(--cursor_pos, 1);
            }
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 18) { // Ctrl+R: Reverse history search (repeat to cycle)
            reset_comp();
            if (!reverse_search_active) {
                reverse_search_query = buf.substr(0, cursor_pos);
                reverse_search_active = true;
                reverse_search_pos = (int)g_env.history.size() - 1;
            }

            int found = -1;
            for (int i = reverse_search_pos; i >= 0; --i) {
                if (reverse_search_query.empty() ||
                    g_env.history[i].find(reverse_search_query) != string::npos) {
                    found = i;
                    break;
                }
            }

            if (found >= 0) {
                hist_idx = found;
                buf = g_env.history[found];
                cursor_pos = buf.size();
                reverse_search_pos = found - 1;
                repaint_line(buf, cursor_pos);
            } else {
                fwrite("\a", 1, 1, stdout);
                fflush(stdout);
            }
            continue;
        }
        if (c == 27) { // Escape: Cancel completion menu
            if (g_comp.active) {
                buf = g_comp.original_buf;
                cursor_pos = g_comp.original_cursor;
                reset_comp();
                repaint_line(buf, cursor_pos);
                continue;
            }
        }
        if (c == 13) { // Enter: Accept completion or submit line
            if (g_comp.active) {
                reset_comp();
                if (!buf.empty() && buf.back() != '/' && buf.back() != '\\') {
                    buf.insert(cursor_pos, 1, ' ');
                    cursor_pos++;
                }
                repaint_line(buf, cursor_pos);
                continue;
            }
            cout << "\n";
            reset_comp();
            return buf;
        }
        if (c == 9) { // Tab: Command/path completion & cycle
            if (!g_comp.active) {
                size_t stem_start = 0;
                string stem = completion_stem(buf, cursor_pos, stem_start);
                bool is_cmd = (stem_start == 0) ||
                              buf.substr(0, stem_start).find_first_not_of(" \t") == string::npos;
                bool has_sep = stem.find_first_of("/\\~") != string::npos;
                vector<string> candidates;
                if (!is_cmd || has_sep) {
                    vector<string> words = tokenize_words(buf.substr(0, cursor_pos));
                    candidates = words.empty() ? vector<string>{} : complete_registered_command(words[0], stem);
                    if (candidates.empty()) candidates = complete_path(stem);
                } else {
                    candidates = complete_command(stem);
                    if (candidates.empty()) candidates = complete_path(stem);
                }

                if (candidates.empty()) {
                    fwrite("\a", 1, 1, stdout);
                    fflush(stdout);
                    continue;
                }

                if (candidates.size() == 1) {
                    string cand = candidates[0];
                    buf.replace(stem_start, cursor_pos - stem_start, cand);
                    cursor_pos = stem_start + cand.size();
                    if (!buf.empty() && buf.back() != '/' && buf.back() != '\\') {
                        buf.insert(cursor_pos, 1, ' ');
                        cursor_pos++;
                    }
                    reset_comp();
                    repaint_line(buf, cursor_pos);
                    continue;
                }

                // Multiple candidates: start interactive menu
                g_comp.active = true;
                g_comp.candidates = candidates;
                g_comp.idx = 0;
                g_comp.stem = stem;
                g_comp.stem_pos = stem_start;
                g_comp.original_buf = buf;
                g_comp.original_cursor = cursor_pos;

                string cp = common_prefix(candidates);
                if (cp.size() > stem.size()) {
                    buf.replace(stem_start, cursor_pos - stem_start, cp);
                    cursor_pos = stem_start + cp.size();
                }

                string cand = g_comp.candidates[g_comp.idx];
                buf.replace(g_comp.stem_pos, cursor_pos - g_comp.stem_pos, cand);
                cursor_pos = g_comp.stem_pos + cand.size();

                render_completion_menu_grid(buf, cursor_pos);
                continue;
            } else {
                // Cycle to next candidate
                g_comp.idx = (g_comp.idx + 1) % g_comp.candidates.size();
                string cand = g_comp.candidates[g_comp.idx];
                buf.replace(g_comp.stem_pos, cursor_pos - g_comp.stem_pos, cand);
                cursor_pos = g_comp.stem_pos + cand.size();
                render_completion_menu_grid(buf, cursor_pos);
                continue;
            }
        }
        else if (c == 8) { // Backspace
            if (g_comp.active) {
                buf = g_comp.original_buf;
                cursor_pos = g_comp.original_cursor;
                reset_comp();
            }
            if (cursor_pos > 0) {
                buf.erase(cursor_pos - 1, 1);
                cursor_pos--;
            }
            repaint_line(buf, cursor_pos);
            continue;
        }
        else if (c == 224 || c == 0) { // Special keys (Arrows, Shift-Tab, Home, End, Delete)
            int arrow = _getch();
            if (g_comp.active) {
                size_t total = g_comp.candidates.size();
                CONSOLE_SCREEN_BUFFER_INFO csbi{};
                int console_width = 80;
                if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
                    console_width = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
                    if (console_width <= 0) console_width = csbi.dwSize.X;
                }
                size_t max_len = 0;
                for (const auto& cand : g_comp.candidates) if (cand.size() > max_len) max_len = cand.size();
                size_t col_width = min(static_cast<size_t>(console_width), max_len + 3);
                size_t num_cols = max(size_t(1), static_cast<size_t>(console_width) / col_width);

                if (arrow == 77) { // Right arrow
                    g_comp.idx = (g_comp.idx + 1) % total;
                } else if (arrow == 75) { // Left arrow
                    g_comp.idx = (g_comp.idx == 0) ? total - 1 : g_comp.idx - 1;
                } else if (arrow == 80) { // Down arrow
                    if (g_comp.idx + num_cols < total) g_comp.idx += num_cols;
                    else g_comp.idx = g_comp.idx % num_cols;
                } else if (arrow == 72) { // Up arrow
                    if (g_comp.idx >= num_cols) g_comp.idx -= num_cols;
                    else {
                        size_t last_row = (total - 1) / num_cols;
                        size_t target = last_row * num_cols + (g_comp.idx % num_cols);
                        g_comp.idx = (target < total) ? target : (total - 1);
                    }
                } else if (arrow == 15) { // Shift-Tab
                    g_comp.idx = (g_comp.idx == 0) ? total - 1 : g_comp.idx - 1;
                } else {
                    reset_comp();
                    repaint_line(buf, cursor_pos);
                    continue;
                }
                string cand = g_comp.candidates[g_comp.idx];
                buf.replace(g_comp.stem_pos, cursor_pos - g_comp.stem_pos, cand);
                cursor_pos = g_comp.stem_pos + cand.size();
                render_completion_menu_grid(buf, cursor_pos);
                continue;
            }
            if (arrow == 75 && cursor_pos > 0) cursor_pos--;
            else if (arrow == 77 && cursor_pos < buf.length()) cursor_pos++;
            else if (arrow == 71) cursor_pos = 0;                             // Home
            else if (arrow == 79) cursor_pos = buf.length();                  // End
            else if (arrow == 83 && cursor_pos < buf.length()) {              // Delete
                buf.erase(cursor_pos, 1);
            } else if (arrow == 72 && hist_idx > 0) {
                hist_idx--; buf = g_env.history[hist_idx]; cursor_pos = buf.length();
            } else if (arrow == 80) {
                if (hist_idx < (int)g_env.history.size() - 1) {
                    hist_idx++; buf = g_env.history[hist_idx]; cursor_pos = buf.length();
                } else { hist_idx = (int)g_env.history.size(); buf = ""; cursor_pos = 0; }
            }
            repaint_line(buf, cursor_pos);
            continue;
        }
        else if (c >= 32 && c <= 126) {
            if (g_comp.active) {
                reset_comp();
            }
            buf.insert(cursor_pos, 1, (char)c); cursor_pos++; repaint_line(buf, cursor_pos);
        }
    }
}
