#include "builtins.hpp"
#include "engine.hpp"
#include "parser.hpp"
#include "expansion.hpp"
#include "jobs.hpp"
#include "scripting.hpp"
#include "terminal.hpp"

const vector<string>& zsh_builtin_command_names() {
    static const vector<string> names = {
        "alias","autoload","bg","bindkey","break","builtin","cd","chdir","clear","cls","command","continue","coproc",
        "compadd","compdef","compinit","declare","dir","dirs","disown","echo","emulate","eval","exec","exit","export",
        "false","fc","fg","getopts","help","history","integer","jobs","kill","let","local","logout","nocorrect",
        "popd","print","printf","pushd","pwd","read","readonly","rehash","repeat","return","select","set","setopt","shift",
        "source","strftime","sysread","syswrite","test","time","times","trap","true","type","typeset","unalias","unset","unsetopt",
        "until","wait","whence","which","zle","zmodload","zstyle",".","[","[["
    };
    return names;
}

bool is_zsh_builtin_command(const string& name) {
    static const set<string> names(zsh_builtin_command_names().begin(), zsh_builtin_command_names().end());
    return names.count(name) != 0;
}

static const char* kZshHelpBuiltinsBody = R"(
  NAVIGATION & FILESYSTEM
  cd / chdir [dir|-]         Change directory ('-' returns to $OLDPWD).
  pwd                        Print current directory (UNIX-style path).
  pushd <dir>                Push cwd onto stack and cd to <dir>.
  popd                       Pop top of dir stack and cd there.
  dirs                       Print directory stack.

  OUTPUT
  echo [-n] [-e] [text]      Print text; -n omits newline, -e enables escapes.
  print [-n] [-l] [-P] [..] Print text; -l one arg per line, -P prompt expand.
  printf <fmt> [args]        C-style formatted output (%s %d %f %x %o %c %%).

  VARIABLES & ENVIRONMENT
  export VAR=VAL             Set variable and export it to child processes.
  unset VAR                  Remove variable, array, or environment entry.
  typeset -a arr=(...)       Declare indexed array (1-based).
  typeset -A map=(...)       Declare associative array (key-value).
    local VAR=VAL              Declare a function-local variable.
    readonly VAR=VAL           Set and protect a variable from mutation.
  integer VAR=N              Declare integer-typed variable.
  let expr                   Arithmetic: let x=5+3  x+=2  x++  x--.

    WINDOWS REGISTRY NAMESPACES
    ${HKLM.Key.Path.Value}     Read a Windows Registry value.
    ${HKCU.Key/With.Dots.Value} Use / to preserve literal periods in key names.
    HKCU.Key.Path.Value=TEXT   Write existing REG_SZ/REG_EXPAND_SZ values.
    HKLM.Key.Path.Value=N      Preserve existing REG_DWORD/REG_QWORD types.
                                                            Unsupported Registry types are rejected.

  ALIASES & OPTIONS
  alias [name=val]           List all aliases or define one.
  unalias <name>             Remove an alias.
  setopt <option>            Enable a shell option flag.
  unsetopt <option>          Disable a shell option flag.

  FLOW CONTROL
  source / . <file>          Execute script in current shell context.
  eval "cmd"                 Evaluate and execute a string as a command.
  return [n]                 Exit current script with status n.
  exit [n]                   Exit the shell with status n.
    logout [n]                 Exit a login shell with status n.
  true                       Return exit status 0.
  false                      Return exit status 1.
  test / [ expr ]            Evaluate condition (see 'help test').
    [[ expr ]]                 Extended conditionals (regex/glob/logical ops).

  HISTORY & RECALL
  history [-c]               List history with line numbers; -c clears it.
  fc [-l] [n]                List last 16 entries (-l) or re-run entry n.
    read [-rAs] [-d c|-N n] [-t s] [-p] [name ...]
                                                         Read fields, arrays, exact bytes, or coprocess data.

  JOB CONTROL
  jobs                       List background jobs.
    fg [%n|%+|%-|%?text]       Resume and foreground the selected job.
    bg [%n|%+|%-|%?text]       Resume the selected job in background.
  disown [%n]                Remove job n from the jobs table.
  wait [pid]                 Wait for background job(s) to finish.
  kill <pid> [..]            Terminate process(es) by PID.

  PROCESS & TIMING
  times                      Print shell user/kernel CPU time.
  getopts <optstr> <var>     Parse option flags from positional args.
  trap ['cmd'] <sig>         Run cmd when signal/event occurs; list with no args.

  LOOKUP
  which <cmd>                Show path, alias expansion, or function type.
  type <cmd>                 Same as which.
    whence <cmd>               Compatibility alias for type-style lookup.
    command <cmd> [args..]     Execute while bypassing alias/function wrappers.
    builtin <name> [args..]    Invoke a shell builtin directly.

    COMPLETION & EDITOR STATE
    autoload <function>        Register functions for deferred loading.
    compinit                   Scan fpath/FPATH completion definitions.
    compadd [candidate...]     Return candidates from a completion function.
    compdef [fn command...]    Register functional command completion.
    bindkey [-L|-M ...]        Add, copy, remove, or list key bindings.
    zle <-N|-A|-D|-l> ...      Add, copy, remove, or list editor widgets.
    zstyle [-L|-d ...]         Add, remove, or list completion styles.
    zmodload [module...]       Load datetime, system, or parameter features.
    rehash                     Invalidate command-path lookup state.
)";

static const char* kZshHelpTestBody = R"(
  FILE TESTS
    -e file    true if file exists
    -f file    true if regular file
    -d file    true if directory
    -L file    true if symbolic link
    -r file    true if readable
    -w file    true if writable
    -s file    true if non-empty

  STRING TESTS
    -z str     true if str is empty
    -n str     true if str is non-empty
    s1 = s2    true if strings are equal  (also ==)
    s1 != s2   true if strings differ
    s1 < s2    true if s1 < s2 lexicographically
    s1 > s2    true if s1 > s2 lexicographically

  NUMERIC TESTS
    n1 -eq n2  equal            n1 -ne n2  not equal
    n1 -lt n2  less than        n1 -le n2  less than or equal
    n1 -gt n2  greater than     n1 -ge n2  greater than or equal

  COMBINERS
    ! expr     negate
    expr -a expr  logical AND
    expr -o expr  logical OR

    EXTENDED [[ ... ]]
        [[ a == a* ]]            glob-aware string match
        [[ s =~ ^[0-9]+$ ]]      regex match
        [[ -n s && s != x ]]     logical operators (&&, ||, !)

  EXAMPLES
    test -f /etc/hosts && echo exists
    [ "$x" -gt 10 ] && echo big
)";

static const char* kZshHelpArraysBody = R"(
  INDEXED ARRAYS (1-based indexing):
    typeset -a fruits=(apple banana cherry)
    echo $fruits[1]       # Outputs: apple
    echo $#fruits         # Outputs: 3 (array length)

  ASSOCIATIVE ARRAYS (Key-Value Maps):
    typeset -A config=(user admin host localhost)
    echo $config[user]    # Outputs: admin

  PARAMETER MODIFIERS & EXPANSION:
    echo ${NAME:-Guest}   # Fallback default value if NAME is empty
        echo ${VAR#pre*}      # Remove shortest matching prefix pattern
        echo ${VAR%*suf}      # Remove shortest matching suffix pattern
    echo ${VAR//foo/bar}  # Regex/pattern string replacement
    echo ${VAR:l}         # Convert string to lowercase
    echo ${VAR:u}         # Convert string to uppercase
        echo ${(f)VAR}        # Split lines to words (newline -> space)
        echo ${(q)VAR}        # Shell-quote the value
    echo $(( 10 * 5 + 2 ))# Inline mathematical evaluation

    ARRAY OPERATIONS:
        echo $arr[2,4]        # 1-based inclusive slice
        arr+=(item)           # Append one item
        arr+=(a b c)          # Append multiple items
)";

static const char* kZshHelpGlobbingBody = R"(
  WILDCARDS:
    *                     Matches zero or more characters.
    ?                     Matches any single character.

  RECURSIVE GLOBBING:
    **/*.cpp              Recursively search subdirectories for matching files.

  GLOB QUALIFIERS:
    ls *(.)               Filter matches: regular files only.
    ls *(/)               Filter matches: directories only.

  PATH DUAL-SLASH NORMALIZATION:
    Both UNIX slashes (/) and Windows slashes (\) are accepted seamlessly:
      cd /c/Users/        # Equivalent to C:\Users\
      cd C:/Projects/     # Works natively
)";

static const char* kZshHelpPipingBody = R"(
  PIPELINES:
    cmd1 | cmd2 | cmd3    Native Win32 anonymous pipes executing concurrently.

  FILE REDIRECTIONS:
    cmd > out.txt         Overwrite stdout to file.
    cmd >> out.txt        Append stdout to file.
    cmd < in.txt          Redirect stdin from file.
    cmd 2> err.txt        Redirect stderr to file.
        cmd <<< "text"        Here-string stdin feed.
        cmd <<EOF ... EOF     Here-document (script mode).
        cmd 2>&1              Duplicate file descriptors.
        cmd 1>&-              Close stdout for command.
        exec 3>file           Open persistent descriptor 3 (fds 3-9).
        exec 4>&3             Duplicate or close persistent descriptors.
        syswrite -o 4 text    Write through a zsh/system descriptor.
        sysread -i 5 name     Read through a zsh/system descriptor.
        setopt MULTIOS        Stream one producer to every output target.

    SUBSTITUTION:
        echo $(cmd)           Command substitution.
        sort <(cmd)           Process substitution input.
        cmd > >(consumer)     Process substitution output.

  JOB CONTROL & SUBSHELLS:
    ping 127.0.0.1 &      Execute process in background.
        jobs; fg %+; bg %-     Use stable current/previous job markers.
    ( cd / && pwd )       Execute command block inside an isolated subshell.
)";

static const char* kZshHelpManualBody = R"(
USAGE:
  zsh [options] [script_file] [arguments]
  zsh -c "command_string"

COMMAND-LINE OPTIONS:
  -c <command>          Execute command string and exit.
  -v, --version         Print version, copyright, and license, then exit.
  -h, --help            Display this help manual.
    --self-test           Run unit, conformance, pipeline, job, and event tests.

HELP TOPICS:
  help builtins         All built-in commands grouped by category.
  help test             test / [ expression reference.
  help arrays           Arrays, associative arrays, and math syntax.
  help globbing         Wildcards, glob qualifiers, and path handling.
  help piping           Pipelines, redirections, and job control.

SCRIPTING HIGHLIGHTS:
    if / elif / else / fi
    for name in ... ; do ... ; done
    for ((init; cond; step)); do ... ; done
    while ... ; do ... ; done
    case word in pattern) ... ;; pattern2) ... ;& ... esac
    command chaining: ;  &&  ||

KEYBOARD SHORTCUTS:
  Up / Down             Browse command history.
  Left / Right          Move cursor within the line.
  Home / End            Jump to line start or end.
  Backspace             Delete character before cursor.
    Delete                Delete character at cursor.
    Ctrl+A                Jump to line start.
    Ctrl+E                Jump to line end.
    Ctrl+U                Delete from start of line to cursor.
    Ctrl+K                Delete from cursor to end of line.
    Ctrl+W                Delete previous word.
    Ctrl+L                Clear screen and repaint current line.
        Ctrl+C                Cancel current input line (SIGINT trap aware).
        Ctrl+Z                Suspend current background job and trigger TSTP trap.
  Ctrl+R                Reverse history search.
  Tab                   File and command completion.

CONFIGURATION:
  ~/.zshrc              Sourced automatically on startup.
  ~/.zsh_history        Timestamped command history log.
    PROMPT                Left-side prompt format string.
    RPROMPT               Right-side prompt format string (auto-hidden on overlap).

EXAMPLES:
  zsh -c "typeset -a a=(x y z); echo $a[2]"
  zsh -c "let n=10; [ $n -gt 5 ] && echo yes"
    zsh -c "if [[ 123 =~ ^[0-9]+$ ]]; then echo ok; fi"
    zsh -c "case x in x) echo yes ;& *) echo fallthrough ;; esac"
  zsh script.zsh
)";

static const char* kZshHelpManualSeparator = "========================================================================\n";
static const char* kZshHelpManualTitle = "CrossShellZSH v3.7.16\n";
static const char* kZshHelpManualSubtitle = "General Help\n";

static bool print_zsh_help_topic(const string& topic) {
    static const ZshHelpTopicData topics[] = {
        {"builtins", &COLOR_BR_GREEN, "\n=== CrossShellZSH BUILT-IN COMMANDS ===", kZshHelpBuiltinsBody},
        {"test", &COLOR_BR_CYAN, "\n=== TEST / [ EXPRESSIONS ===", kZshHelpTestBody},
        {"arrays", &COLOR_BR_YELLOW, "\n=== ZSH ARRAYS & VARIABLES ===", kZshHelpArraysBody},
        {"globbing", &COLOR_BR_CYAN, "\n=== EXTENDED GLOBBING & QUALIFIERS ===", kZshHelpGlobbingBody},
        {"piping", &COLOR_BR_BLUE, "\n=== PIPELINES, REDIRECTION & JOBS ===", kZshHelpPipingBody}
    };

    for (const auto& entry : topics) {
        if (topic == entry.name) {
            cout << *entry.color << entry.title << COLOR_RESET << entry.body;
            return true;
        }
    }
    return false;
}

void print_comprehensive_help(const string& topic) {
    if (print_zsh_help_topic(topic)) {
        return;
    }

    // Default Full Comprehensive Manual
    cout << COLOR_BR_GREEN << kZshHelpManualSeparator;
    cout << kZshHelpManualTitle;
    cout << kZshHelpManualSubtitle;
    cout << kZshHelpManualSeparator << COLOR_RESET << kZshHelpManualBody;
}

bool builtin_test_eval(const vector<string>& t) {
    if (t.empty()) return false;
    if (t.size() == 1) return !t[0].empty() && t[0] != "0";
    if (t[0] == "!") { return !builtin_test_eval(vector<string>(t.begin()+1, t.end())); }
    if (t.size() == 2) {
        const string& op = t[0]; const string& v = t[1];
        error_code ec;
        if (op=="-e") return fs::exists(normalize_path_to_win(v), ec);
        if (op=="-f") return fs::is_regular_file(normalize_path_to_win(v), ec);
        if (op=="-d") return fs::is_directory(normalize_path_to_win(v), ec);
        if (op=="-L") return fs::is_symlink(normalize_path_to_win(v), ec);
        if (op=="-s") return fs::exists(normalize_path_to_win(v), ec) && fs::file_size(normalize_path_to_win(v), ec) > 0;
        if (op=="-r") { DWORD a=GetFileAttributesW(string_to_wstring(normalize_path_to_win(v)).c_str()); return a!=INVALID_FILE_ATTRIBUTES; }
        if (op=="-w") { DWORD a=GetFileAttributesW(string_to_wstring(normalize_path_to_win(v)).c_str()); return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_READONLY); }
        if (op=="-z") return v.empty();
        if (op=="-n") return !v.empty();
        return !v.empty();
    }
    if (t.size() >= 3) {
        const string& a=t[0]; const string& op=t[1]; const string& b=t[2];
        if (op=="=="||op=="=") return a==b;
        if (op=="!=")          return a!=b;
        if (op=="<")           return a<b;
        if (op==">")           return a>b;
        auto n=[](const string& s)->long long{ try{return stoll(s);}catch(...){return 0;} };
        if (op=="-eq") return n(a)==n(b); if (op=="-ne") return n(a)!=n(b);
        if (op=="-lt") return n(a)<n(b);  if (op=="-le") return n(a)<=n(b);
        if (op=="-gt") return n(a)>n(b);  if (op=="-ge") return n(a)>=n(b);
        if (op=="-a") return builtin_test_eval({a}) && builtin_test_eval(vector<string>(t.begin()+2,t.end()));
        if (op=="-o") return builtin_test_eval({a}) || builtin_test_eval(vector<string>(t.begin()+2,t.end()));
    }
    return false;
}

int builtin_test(const vector<string>& args) {
    vector<string> t(args.begin()+1, args.end());
    if (!t.empty() && t.back()=="]") t.pop_back();
    return builtin_test_eval(t) ? 0 : 1;
}

int builtin_double_bracket(const vector<string>& args) {
    vector<string> t(args.begin() + 1, args.end());
    if (!t.empty() && t.back() == "]]" ) t.pop_back();
    if (t.empty()) return 1;

    size_t idx = 0;

    auto eval_binary = [&](const string& a, const string& op, const string& b) -> bool {
        if (op == "=~") {
            try {
                regex re(b);
                smatch sm;
                bool match_res = regex_search(a, sm, re);
                if (match_res) {
                    g_env.vars["MATCH"] = sm.str(0);
                    vector<string> match_groups;
                    for (size_t gi = 1; gi < sm.size(); ++gi) {
                        match_groups.push_back(sm.str(gi));
                    }
                    g_env.indexed_arrays["match"] = match_groups;
                } else {
                    g_env.vars.erase("MATCH");
                    g_env.indexed_arrays.erase("match");
                }
                return match_res;
            } catch (...) {
                return false;
            }
        }
        if (op == "==" || op == "=") {
            if (b.find_first_of("*?") != string::npos || b.find("@(") != string::npos) return match_wildcard(b, a);
            return a == b;
        }
        if (op == "!=") {
            if (b.find_first_of("*?") != string::npos || b.find("@(") != string::npos) return !match_wildcard(b, a);
            return a != b;
        }
        if (op == "<") return a < b;
        if (op == ">") return a > b;
        if (op == "-nt") {
            error_code ec1, ec2;
            auto t1 = fs::last_write_time(normalize_path_to_win(a), ec1);
            auto t2 = fs::last_write_time(normalize_path_to_win(b), ec2);
            if (ec1 || ec2) return false;
            return t1 > t2;
        }
        if (op == "-ot") {
            error_code ec1, ec2;
            auto t1 = fs::last_write_time(normalize_path_to_win(a), ec1);
            auto t2 = fs::last_write_time(normalize_path_to_win(b), ec2);
            if (ec1 || ec2) return false;
            return t1 < t2;
        }
        if (op == "-ef") {
            error_code ec1, ec2;
            auto c1 = fs::canonical(normalize_path_to_win(a), ec1);
            auto c2 = fs::canonical(normalize_path_to_win(b), ec2);
            if (ec1 || ec2) return false;
            return c1 == c2;
        }
        auto n = [](const string& s) -> long long { try { return stoll(s); } catch (...) { return 0; } };
        if (op == "-eq") return n(a) == n(b);
        if (op == "-ne") return n(a) != n(b);
        if (op == "-lt") return n(a) < n(b);
        if (op == "-le") return n(a) <= n(b);
        if (op == "-gt") return n(a) > n(b);
        if (op == "-ge") return n(a) >= n(b);
        return false;
    };

    function<bool()> parse_expr;
    function<bool()> parse_and;
    function<bool()> parse_atom;

    parse_atom = [&]() -> bool {
        if (idx >= t.size()) return false;

        if (t[idx] == "!") {
            ++idx;
            return !parse_atom();
        }

        if (t[idx] == "(") {
            ++idx;
            bool v = parse_expr();
            if (idx < t.size() && t[idx] == ")") ++idx;
            return v;
        }

        if (idx + 1 < t.size()) {
            string op = t[idx];
            static const set<string> unary_ops = {"-e", "-f", "-d", "-L", "-s", "-r", "-w", "-x", "-n", "-z"};
            if (unary_ops.count(op)) {
                idx += 2;
                string v = t[idx - 1];
                error_code ec;
                if (op == "-e") return fs::exists(normalize_path_to_win(v), ec);
                if (op == "-f") return fs::is_regular_file(normalize_path_to_win(v), ec);
                if (op == "-d") return fs::is_directory(normalize_path_to_win(v), ec);
                if (op == "-L") return fs::is_symlink(normalize_path_to_win(v), ec);
                if (op == "-s") return fs::exists(normalize_path_to_win(v), ec) && fs::file_size(normalize_path_to_win(v), ec) > 0;
                if (op == "-r") { DWORD a = GetFileAttributesW(string_to_wstring(normalize_path_to_win(v)).c_str()); return a != INVALID_FILE_ATTRIBUTES; }
                if (op == "-w") { DWORD a = GetFileAttributesW(string_to_wstring(normalize_path_to_win(v)).c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_READONLY); }
                if (op == "-x") {
                    string norm = normalize_path_to_win(v);
                    if (fs::exists(norm, ec) && !fs::is_directory(norm, ec)) {
                        string ext = fs::path(norm).extension().string();
                        transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                        return (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == "");
                    }
                    return false;
                }
                if (op == "-n") return !v.empty();
                if (op == "-z") return v.empty();
            }
        }

        if (idx + 2 < t.size()) {
            string a = t[idx], op = t[idx + 1], b = t[idx + 2];
            static const set<string> ops = {
                "=~", "==", "=", "!=", "<", ">", "-nt", "-ot", "-ef",
                "-eq", "-ne", "-lt", "-le", "-gt", "-ge"
            };
            if (ops.count(op)) {
                idx += 3;
                return eval_binary(a, op, b);
            }
        }

        string v = t[idx++];
        return !v.empty() && v != "0";
    };

    parse_and = [&]() -> bool {
        bool v = parse_atom();
        while (idx < t.size() && t[idx] == "&&") {
            ++idx;
            bool rhs = parse_atom();
            v = v && rhs;
        }
        return v;
    };

    parse_expr = [&]() -> bool {
        bool v = parse_and();
        while (idx < t.size() && t[idx] == "||") {
            ++idx;
            bool rhs = parse_and();
            v = v || rhs;
        }
        return v;
    };

    bool ok = parse_expr();
    return (ok && idx == t.size()) ? 0 : 1;
}

int builtin_printf(const vector<string>& args) {
    if (args.size() < 2) { cerr << "printf: usage: printf format [args]\n"; return 1; }
    const string& fmt = args[1]; size_t ai = 2;
    bool first_pass = true;
    do {
        size_t args_before_pass = ai;
        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] == '\\' && i + 1 < fmt.size()) {
                switch (fmt[++i]) {
                    case 'n': cout << '\n'; break; case 't': cout << '\t'; break;
                    case 'r': cout << '\r'; break; case '\\': cout << '\\'; break;
                    case 'a': cout << '\a'; break; case 'b': cout << '\b'; break;
                    default: cout << '\\' << fmt[i]; break;
                }
            } else if (fmt[i] == '%' && i + 1 < fmt.size()) {
                if (fmt[i + 1] == '%') { cout << '%'; ++i; continue; }
                size_t spec_start = i;
                size_t j = i + 1;
                while (j < fmt.size() && string("0123456789-+ #.").find(fmt[j]) != string::npos) ++j;
                if (j < fmt.size() && string("sdfxXocu").find(fmt[j]) != string::npos) {
                    char conv = fmt[j];
                    string sub_fmt = fmt.substr(spec_start, j - spec_start + 1);
                    i = j;
                    string val = ai < args.size() ? args[ai++] : "";
                    char buf[1024] = {};
                    if (conv == 's') {
                        snprintf(buf, sizeof(buf), sub_fmt.c_str(), val.c_str());
                    } else if (conv == 'f') {
                        double d = 0.0; try { d = stod(val); } catch (...) {}
                        snprintf(buf, sizeof(buf), sub_fmt.c_str(), d);
                    } else if (conv == 'c') {
                        char c = val.empty() ? '\0' : val[0];
                        snprintf(buf, sizeof(buf), sub_fmt.c_str(), c);
                    } else {
                        long long n = 0; try { n = stoll(val); } catch (...) {}
                        string ll_fmt = sub_fmt;
                        ll_fmt.insert(ll_fmt.size() - 1, "ll");
                        snprintf(buf, sizeof(buf), ll_fmt.c_str(), n);
                    }
                    cout << buf;
                } else {
                    cout << '%';
                }
            } else {
                cout << fmt[i];
            }
        }
        first_pass = false;
        if (ai == args_before_pass) break;
    } while (first_pass || ai < args.size());
    return 0;
}

int builtin_print(const vector<string>& args) {
    bool newline=true, per_line=false, prompt_exp=false, to_coprocess=false;
    int target_fd = 1;
    size_t start = 1;
    for (; start < args.size(); ++start) {
        if (args[start]=="-n") newline=false;
        else if (args[start]=="-l") per_line=true;
        else if (args[start]=="-P") prompt_exp=true;
        else if (args[start]=="-p") to_coprocess=true;
        else if (args[start]=="-u" && start + 1 < args.size()) {
            try { target_fd = std::stoi(args[++start]); } catch (...) {}
        } else if (args[start].size() >= 3 && args[start].rfind("-u", 0) == 0 && isdigit(static_cast<unsigned char>(args[start][2]))) {
            try { target_fd = std::stoi(args[start].substr(2)); } catch (...) {}
        }
        else if (args[start]=="--") { start++; break; }
        else break;
    }
    if (to_coprocess) {
        if (!g_coprocess.input) {
            cerr << "print: no active coprocess input\n";
            return 1;
        }
        string payload;
        for (size_t i = start; i < args.size(); ++i) {
            payload += args[i];
            if (per_line) payload += '\n';
            else if (i + 1 < args.size()) payload += ' ';
        }
        if (newline && !per_line) payload += '\n';
        DWORD total_written = 0;
        while (total_written < payload.size()) {
            DWORD written = 0;
            if (!WriteFile(g_coprocess.input, payload.data() + total_written,
                           static_cast<DWORD>(payload.size() - total_written), &written, nullptr) || written == 0) {
                return 1;
            }
            total_written += written;
        }
        return 0;
    }
    ostream& out = (target_fd == 2) ? cerr : cout;
    for (size_t i = start; i < args.size(); ++i) {
        if (prompt_exp) {
            // render the arg as a prompt format string
            string saved = g_env.vars["PROMPT"];
            g_env.vars["PROMPT"] = args[i];
            out << render_prompt();
            g_env.vars["PROMPT"] = saved;
        } else out << args[i];
        if (per_line) out << '\n';
        else if (i+1 < args.size()) out << ' ';
    }
    if (newline && !per_line) out << '\n';
    return 0;
}

int builtin_cd(const vector<string>& args) {
    string target = args.size() > 1 ? args[1] : g_env.vars["HOME"];
    if (target == "~") {
        target = g_env.vars["HOME"];
    } else if (target.size() > 1 && target[0] == '~' && (target[1] == '/' || target[1] == '\\')) {
        string home = g_env.vars["HOME"];
        if (!home.empty() && home.back() == '\\') home.pop_back();
        string rest = target.substr(2);
        target = home + "\\" + normalize_path_to_win(rest);
    }
    target = normalize_path_to_win(target);
    if (target.size() == 2 && isalpha(static_cast<unsigned char>(target[0])) && target[1] == ':') {
        target += "\\";
    }
    if (target == "-") { target = g_env.oldpwd; cout << target << "\n"; }
    try {
        string current = fs::current_path().string();
        fs::current_path(target);

        g_env.oldpwd = current;
        g_env.vars["OLDPWD"] = normalize_path_to_unix(current);
        g_env.vars["PWD"] = normalize_path_to_unix(fs::current_path().string());

        SetEnvironmentVariableA("OLDPWD", g_env.vars["OLDPWD"].c_str());
        SetEnvironmentVariableA("PWD", g_env.vars["PWD"].c_str());

        g_env.prompt_dirty = true;
        return 0;
    } catch (...) {
        cout << COLOR_RED << "cd: no such file or directory: " << target << COLOR_RESET << "\n";
        return 1;
    }
}

int builtin_echo(const vector<string>& args) {
    bool newline = true, enable_escapes = false;
    size_t start = 1;
    while (start < args.size() && args[start][0] == '-') {
        string flag = args[start];
        if (flag == "-n") newline = false;
        else if (flag == "-e") enable_escapes = true;
        else if (flag == "-E") enable_escapes = false;
        else break;
        start++;
    }
    for (size_t i = start; i < args.size(); ++i) {
        string str = args[i];
        if (enable_escapes) {
            try { str = regex_replace(str, regex(R"(\\n)"), "\n"); } catch (...) {}
            try { str = regex_replace(str, regex(R"(\\t)"), "\t"); } catch (...) {}
            try { str = regex_replace(str, regex(R"(\\e)"), "\033"); } catch (...) {}
        }
        cout << str << (i + 1 < args.size() ? " " : "");
    }
    if (newline) cout << "\n";
    return 0;
}

static int builtin_coproc(const vector<string>& args) {
    if (args.size() == 2 && args[1] == "-") {
        close_coprocess(true);
        return 0;
    }
    if (args.size() == 2 && args[1] == "-i") {
        close_handle_if_valid(g_coprocess.input);
        g_coprocess.input = nullptr;
        return 0;
    }
    if (args.size() < 2) {
        cerr << "coproc: command required\n";
        return 1;
    }

    close_coprocess(true);
    string executable = find_executable_in_path(args[1]);
    if (executable.empty()) {
        cerr << "zsh: command not found: " << args[1] << "\n";
        return 127;
    }

    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE child_input = nullptr;
    HANDLE parent_input = nullptr;
    HANDLE parent_output = nullptr;
    HANDLE child_output = nullptr;
    if (!CreatePipe(&child_input, &parent_input, &security, 0) ||
        !CreatePipe(&parent_output, &child_output, &security, 0)) {
        close_handle_if_valid(child_input);
        close_handle_if_valid(parent_input);
        close_handle_if_valid(parent_output);
        close_handle_if_valid(child_output);
        cerr << "coproc: failed to create pipes\n";
        return 1;
    }
    SetHandleInformation(parent_input, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(parent_output, HANDLE_FLAG_INHERIT, 0);

    string command_line = win_quote_arg(executable);
    for (size_t i = 2; i < args.size(); ++i) command_line += " " + win_quote_arg(args[i]);
    wstring wide_command = string_to_wstring(command_line);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_input;
    startup.hStdOutput = child_output;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    BOOL created = CreateProcessW(nullptr, wide_command.data(), nullptr, nullptr, TRUE, 0,
                                  nullptr, nullptr, &startup, &process);
    close_handle_if_valid(child_input);
    close_handle_if_valid(child_output);
    if (!created) {
        close_handle_if_valid(parent_input);
        close_handle_if_valid(parent_output);
        cerr << "coproc: failed to start: " << args[1] << "\n";
        return 1;
    }

    CloseHandle(process.hThread);
    g_coprocess.process = process.hProcess;
    g_coprocess.input = parent_input;
    g_coprocess.output = parent_output;
    g_coprocess.pid = process.dwProcessId;
    g_env.vars["COPROC_PID"] = to_string(process.dwProcessId);
    return 0;
}

int builtin_read(const vector<string>& args) {
    bool silent = false;
    bool raw = false;
    bool array = false;
    bool from_coprocess = false;
    bool exact = false;
    bool has_timeout = false;
    size_t exact_count = 0;
    double timeout_seconds = 0.0;
    char delimiter = '\n';
    vector<string> target_vars;

    for (size_t i = 1; i < args.size(); ++i) {
        const string& option = args[i];
        if (option == "--") {
            target_vars.insert(target_vars.end(), args.begin() + static_cast<ptrdiff_t>(i + 1), args.end());
            break;
        }
        if (option == "-s") silent = true;
        else if (option == "-r") raw = true;
        else if (option == "-A") array = true;
        else if (option == "-p") from_coprocess = true;
        else if ((option == "-d" || option == "-N" || option == "-t") && i + 1 < args.size()) {
            const string value = args[++i];
            if (option == "-d") {
                delimiter = value.empty() ? '\0' : value[0];
            } else if (option == "-N") {
                char* end = nullptr;
                unsigned long long count = strtoull(value.c_str(), &end, 10);
                if (end == value.c_str() || *end != '\0' || count == 0 || count > SIZE_MAX) {
                    cerr << "read: invalid character count: " << value << "\n";
                    return 1;
                }
                exact = true;
                exact_count = static_cast<size_t>(count);
            } else {
                char* end = nullptr;
                timeout_seconds = strtod(value.c_str(), &end);
                if (end == value.c_str() || *end != '\0' || timeout_seconds < 0.0) {
                    cerr << "read: invalid timeout: " << value << "\n";
                    return 1;
                }
                has_timeout = true;
            }
        } else if (!option.empty() && option[0] == '-') {
            cerr << "read: unsupported option: " << option << "\n";
            return 1;
        } else {
            target_vars.push_back(option);
        }
    }

    if (target_vars.empty()) target_vars.push_back("REPLY");
    for (const auto& name : target_vars) {
        if (!is_valid_env_var_name(name)) {
            cerr << "read: invalid variable name: " << name << "\n";
            return 1;
        }
        if (g_env.readonly_vars.count(name)) {
            cerr << "zsh: read-only variable: " << name << "\n";
            return 1;
        }
    }
    if (array && target_vars.size() != 1) {
        cerr << "read: -A requires one array name\n";
        return 1;
    }
    if (from_coprocess && !g_coprocess.output) return 1;

    DWORD saved_console_mode = 0;
    HANDLE standard_input = GetStdHandle(STD_INPUT_HANDLE);
    bool restore_console_mode = silent && GetConsoleMode(standard_input, &saved_console_mode);
    if (restore_console_mode) SetConsoleMode(standard_input, saved_console_mode & ~ENABLE_ECHO_INPUT);

    const ULONGLONG start_tick = GetTickCount64();
    const ULONGLONG timeout_ms = has_timeout
        ? static_cast<ULONGLONG>(timeout_seconds * 1000.0 + 0.5)
        : INFINITE;
    auto read_coprocess_byte = [&](char& byte) -> bool {
        while (true) {
            DWORD available = 0;
            if (!PeekNamedPipe(g_coprocess.output, nullptr, 0, nullptr, &available, nullptr)) return false;
            if (available > 0) {
                DWORD bytes_read = 0;
                return ReadFile(g_coprocess.output, &byte, 1, &bytes_read, nullptr) && bytes_read == 1;
            }
            if (has_timeout && GetTickCount64() - start_tick >= timeout_ms) return false;
            if (g_coprocess.process && WaitForSingleObject(g_coprocess.process, 1) == WAIT_OBJECT_0) {
                if (!PeekNamedPipe(g_coprocess.output, nullptr, 0, nullptr, &available, nullptr) || available == 0) return false;
            }
            Sleep(1);
        }
    };

    cin.clear();
    string input;
    bool completed = false;
    bool escaped = false;
    while (!exact || input.size() < exact_count) {
        char byte = 0;
        bool got_byte = from_coprocess ? read_coprocess_byte(byte) : static_cast<bool>(cin.get(byte));
        if (!got_byte) break;

        if (!exact && byte == delimiter && !escaped) {
            completed = true;
            break;
        }
        if (!exact && delimiter == '\n' && byte == '\r' && !escaped) continue;
        if (!raw && !escaped && byte == '\\') {
            escaped = true;
            continue;
        }
        if (escaped) {
            escaped = false;
            if (byte == '\n') continue;
        }
        input += byte;
    }
    if (exact && input.size() == exact_count) completed = true;
    if (escaped) input += '\\';

    if (restore_console_mode) {
        SetConsoleMode(standard_input, saved_console_mode);
        cout << "\n";
    } else if (silent) {
        cout << "\n";
    }

    if (!completed) return 1;

    auto ifs_fields = [&](const string& value) {
        string ifs = g_env.vars.count("IFS") ? g_env.vars["IFS"] : " \t\n";
        vector<pair<size_t, size_t>> spans;
        size_t position = 0;
        while (position < value.size()) {
            while (position < value.size() && ifs.find(value[position]) != string::npos) ++position;
            if (position >= value.size()) break;
            size_t begin = position;
            while (position < value.size() && ifs.find(value[position]) == string::npos) ++position;
            spans.push_back({begin, position});
        }
        return spans;
    };
    vector<pair<size_t, size_t>> fields = ifs_fields(input);

    if (array) {
        vector<string> values;
        values.reserve(fields.size());
        for (const auto& field : fields) values.push_back(input.substr(field.first, field.second - field.first));
        g_env.vars.erase(target_vars[0]);
        g_env.assoc_arrays.erase(target_vars[0]);
        g_env.indexed_arrays[target_vars[0]] = std::move(values);
        return 0;
    }

    if (target_vars.size() == 1) {
        g_env.indexed_arrays.erase(target_vars[0]);
        g_env.assoc_arrays.erase(target_vars[0]);
        g_env.vars[target_vars[0]] = input;
        return 0;
    }
    for (size_t i = 0; i < target_vars.size(); ++i) {
        string value;
        if (i < fields.size()) {
            size_t begin = fields[i].first;
            size_t end = fields[i].second;
            if (i + 1 == target_vars.size() && fields.size() > target_vars.size()) end = fields.back().second;
            value = input.substr(begin, end - begin);
        }
        g_env.indexed_arrays.erase(target_vars[i]);
        g_env.assoc_arrays.erase(target_vars[i]);
        g_env.vars[target_vars[i]] = std::move(value);
    }
    return 0;
}

int builtin_typeset(const vector<string>& args) {
    if (args.size() < 2) return 1;
    bool is_array = false;
    bool is_assoc = false;
    bool is_unique = false;
    bool is_integer = false;
    bool is_readonly = false;

    size_t i = 1;
    while (i < args.size() && !args[i].empty() && args[i][0] == '-' && args[i] != "--") {
        const string& flag = args[i];
        if (flag.find('a') != string::npos) is_array = true;
        if (flag.find('A') != string::npos) is_assoc = true;
        if (flag.find('U') != string::npos) is_unique = true;
        if (flag.find('i') != string::npos) is_integer = true;
        if (flag.find('r') != string::npos) is_readonly = true;
        ++i;
    }
    if (i < args.size() && args[i] == "--") ++i;
    if (i >= args.size()) return 0;

    if (is_array || is_assoc) {
        string declaration = args[i];
        size_t equal = declaration.find('=');
        string name = equal == string::npos ? declaration : declaration.substr(0, equal);
        if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
        mark_function_local(name);
        string payload = equal == string::npos ? "" : declaration.substr(equal + 1);
        for (size_t k = i + 1; k < args.size(); ++k) {
            if (!payload.empty()) payload += ' ';
            payload += args[k];
        }
        payload = trim_copy(payload);
        if (!payload.empty() && payload.front() == '(') payload.erase(payload.begin());
        if (!payload.empty() && payload.back() == ')') payload.pop_back();
        vector<string> values = tokenize_words(payload);

        if (is_assoc) {
            map<string, string> entries;
            for (size_t k = 0; k + 1 < values.size(); k += 2) entries[values[k]] = values[k + 1];
            g_env.assoc_arrays[name] = std::move(entries);
        } else {
            if (is_unique) {
                set<string> seen;
                values.erase(remove_if(values.begin(), values.end(), [&](const string& value) {
                    return !seen.insert(value).second;
                }), values.end());
                g_env.unique_arrays.insert(name);
            } else {
                g_env.unique_arrays.erase(name);
            }
            g_env.indexed_arrays[name] = std::move(values);
            g_env.integer_vars.erase(name);
        }
        if (is_readonly) g_env.readonly_vars.insert(name);
        return 0;
    }

    for (size_t k = i; k < args.size(); ++k) {
        size_t equal = args[k].find('=');
        string name = equal == string::npos ? args[k] : args[k].substr(0, equal);
        if (!is_valid_env_var_name(name)) { cerr << "typeset: invalid variable name: " << name << "\n"; return 1; }
        if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
        mark_function_local(name);
        if (is_integer) g_env.integer_vars.insert(name);
        if (equal != string::npos) {
            string value = g_env.expand_vars(args[k].substr(equal + 1));
            g_env.vars[name] = g_env.integer_vars.count(name) ? to_string(eval_math_expr(value)) : value;
        } else if (!g_env.vars.count(name)) {
            g_env.vars[name] = is_integer ? "0" : "";
        }
        if (is_readonly) g_env.readonly_vars.insert(name);
    }
    return 0;
}

static void close_background_job_handles(BackgroundJob& job) {
    for (HANDLE process : job.processes) close_handle_if_valid(process);
    close_handle_if_valid(job.hJob);
    job.hProcess = nullptr;
    job.hJob = nullptr;
    job.processes.clear();
    job.pids.clear();
}

static void wait_for_background_job(const BackgroundJob& job) {
    for (HANDLE process : job.processes) {
        if (process) WaitForSingleObject(process, INFINITE);
    }
}

const char* job_state_name(JobState state) {
    switch (state) {
        case JobState::Running: return "running";
        case JobState::Stopped: return "stopped";
        case JobState::Done: return "done";
        case JobState::Failed: return "failed";
    }
    return "unknown";
}

static void refresh_background_job_state(BackgroundJob& job) {
    if (job.state == JobState::Stopped || job.state == JobState::Failed) return;
    DWORD status = 0;
    if (!job.hProcess || !GetExitCodeProcess(job.hProcess, &status)) {
        job.state = JobState::Failed;
        job.launch_error = GetLastError();
        return;
    }
    job.exit_code = status;
    job.state = status == STILL_ACTIVE ? JobState::Running : JobState::Done;
}

static void repair_job_markers() {
    auto exists = [](unsigned long id) {
        return id != 0 && any_of(g_env.jobs.begin(), g_env.jobs.end(),
                                 [&](const BackgroundJob& job) { return job.job_id == id; });
    };
    if (!exists(g_current_job_id)) {
        g_current_job_id = exists(g_previous_job_id) ? g_previous_job_id :
            (g_env.jobs.empty() ? 0 : g_env.jobs.back().job_id);
    }
    if (!exists(g_previous_job_id) || g_previous_job_id == g_current_job_id) {
        g_previous_job_id = 0;
        for (auto it = g_env.jobs.rbegin(); it != g_env.jobs.rend(); ++it) {
            if (it->job_id != g_current_job_id) { g_previous_job_id = it->job_id; break; }
        }
    }
}

bool resolve_job_index(const string& spec, size_t& index) {
    if (g_env.jobs.empty()) return false;
    if (spec.empty() || spec == "%" || spec == "%%" || spec == "%+") {
        repair_job_markers();
        for (size_t i = 0; i < g_env.jobs.size(); ++i)
            if (g_env.jobs[i].job_id == g_current_job_id) { index = i; return true; }
        return false;
    }
    if (spec == "%-") {
        repair_job_markers();
        for (size_t i = 0; i < g_env.jobs.size(); ++i)
            if (g_env.jobs[i].job_id == g_previous_job_id) { index = i; return true; }
        return false;
    }
    if (spec.rfind("%?", 0) == 0) {
        string needle = spec.substr(2);
        if (needle.empty()) return false;
        for (size_t i = g_env.jobs.size(); i > 0; --i) {
            if (g_env.jobs[i - 1].command.find(needle) != string::npos) {
                index = i - 1;
                return true;
            }
        }
        return false;
    }

    string number = spec;
    if (!number.empty() && number[0] == '%') number.erase(number.begin());
    char* end = nullptr;
    errno = 0;
    unsigned long parsed = strtoul(number.c_str(), &end, 10);
    if (errno != 0 || end == number.c_str() || *end != '\0' || parsed == 0) return false;
    for (size_t i = 0; i < g_env.jobs.size(); ++i) {
        if (g_env.jobs[i].job_id == parsed) { index = i; return true; }
    }
    return false;
}

int builtin_jobs(const vector<string>& args) {
    bool long_format = false;
    bool pid_only = false;
    bool end_of_options = false;
    vector<size_t> requested_job_indices;

    for (size_t i = 1; i < args.size(); ++i) {
        const string& token = args[i];
        if (!end_of_options && token == "--") {
            end_of_options = true;
            continue;
        }
        if (!end_of_options && token.size() > 1 && token[0] == '-') {
            for (size_t flag_index = 1; flag_index < token.size(); ++flag_index) {
                if (token[flag_index] == 'l') long_format = true;
                else if (token[flag_index] == 'p') pid_only = true;
                else {
                    cerr << "zsh: jobs: invalid option: " << token << "\n";
                    return 1;
                }
            }
            continue;
        }

        size_t job_index = 0;
        if (!resolve_job_index(token, job_index)) {
            cerr << "zsh: jobs: invalid job id: " << token << "\n";
            return 1;
        }
        requested_job_indices.push_back(job_index);
    }

    for (size_t i = 0; i < g_env.jobs.size(); ++i) {
        if (!requested_job_indices.empty() &&
            find(requested_job_indices.begin(), requested_job_indices.end(), i) == requested_job_indices.end()) {
            continue;
        }

        refresh_background_job_state(g_env.jobs[i]);
        string state = job_state_name(g_env.jobs[i].state);
        char marker = g_env.jobs[i].job_id == g_current_job_id ? '+' :
                      (g_env.jobs[i].job_id == g_previous_job_id ? '-' : ' ');
        if (pid_only) {
            cout << g_env.jobs[i].pid << "\n";
        } else if (long_format) {
            cout << "[" << g_env.jobs[i].job_id << "]  " << marker << " " << g_env.jobs[i].pid << " " << state
                 << "      " << g_env.jobs[i].command << "\n";
        } else {
            cout << "[" << g_env.jobs[i].job_id << "]  " << marker << " " << state << "      " << g_env.jobs[i].command << "\n";
        }
    }
    return 0;
}

int builtin_help(const vector<string>& args) {
    string topic = args.size() > 1 ? args[1] : "";
    print_comprehensive_help(topic);
    return 0;
}

static int execute_condition_loop(const string& block, bool until_mode);
static int execute_repeat_loop(const string& block);
static int execute_select_loop(const string& block);
static int execute_timed_command(const string& block);

static int builtin_emulate(const vector<string>& args) {
    if (args.size() == 1) {
        bool is_sh = g_env.options.count("shwordsplit") && g_env.options.at("shwordsplit");
        cout << (is_sh ? "sh" : "zsh") << "\n";
        return 0;
    }
    bool reset = false;
    bool local_opts = false;
    string mode;
    for (size_t i = 1; i < args.size(); ++i) {
        const string& a = args[i];
        if (a == "-R") reset = true;
        else if (a == "-L") local_opts = true;
        else if (a == "-LR" || a == "-RL") { local_opts = true; reset = true; }
        else if (!a.empty() && a[0] != '-') {
            mode = canonicalize_option_name(a);
        }
    }
    if (mode.empty()) {
        cerr << "emulate: mode required\n";
        return 1;
    }
    if (local_opts && !g_function_local_scopes.empty()) {
        if (!g_function_local_scopes.back().local_options) {
            g_function_local_scopes.back().local_options = true;
            g_function_local_scopes.back().options_before = g_env.options;
        }
    }
    if (reset) {
        g_env.options.clear();
    }
    if (mode == "zsh") {
        g_env.options["shwordsplit"] = false;
        g_env.options["ksharrays"] = false;
        g_env.options["posixbuiltins"] = false;
        g_env.options["multios"] = true;
        g_env.options["extendedglob"] = true;
        g_env.options["shglob"] = false;
        g_env.options["autocd"] = true;
        g_env.options["promptsubst"] = true;
    } else if (mode == "sh" || mode == "ksh") {
        g_env.options["shwordsplit"] = true;
        g_env.options["ksharrays"] = true;
        g_env.options["posixbuiltins"] = true;
        g_env.options["multios"] = false;
        g_env.options["extendedglob"] = false;
        g_env.options["shglob"] = true;
        g_env.options["autocd"] = false;
    } else if (mode == "csh") {
        g_env.options["shwordsplit"] = true;
        g_env.options["ksharrays"] = false;
        g_env.options["cshnullglob"] = true;
        g_env.options["multios"] = false;
        g_env.options["extendedglob"] = false;
    } else {
        cerr << "emulate: unknown emulation mode: " << mode << "\n";
        return 1;
    }
    return 0;
}

int dispatch_command(vector<string> args) {
    if (args.empty()) return 0;

    auto assignment_value = [&](const string& raw, bool integer_attribute) {
        string value = g_env.expand_vars(raw);
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        return integer_attribute ? to_string(eval_math_expr(value)) : value;
    };

    if (args.size() == 1) {
        smatch array_assignment;
        static const regex array_assignment_re(R"(^([A-Za-z_][A-Za-z0-9_]*)=\(([\s\S]*)\)$)");
        if (regex_match(args[0], array_assignment, array_assignment_re)) {
            string name = array_assignment[1].str();
            if (g_env.readonly_vars.count(name)) {
                cerr << "zsh: read-only variable: " << name << "\n";
                return 1;
            }
            vector<string> values = tokenize_words(array_assignment[2].str());
            for (auto& value : values) value = g_env.expand_vars(value);
            g_env.vars.erase(name);
            g_env.assoc_arrays.erase(name);
            g_env.integer_vars.erase(name);
            g_env.indexed_arrays[name] = std::move(values);
            return 0;
        }
    }

    // Support assignment prefixes like VAR=val cmd arg and standalone VAR=val.
    auto is_assignment_word = [](const string& tok) -> bool {
        static const regex assign_re(R"(^([A-Za-z_][A-Za-z0-9_]*|HKLM\..+|HKCU\..+)=.*$)");
        return regex_match(tok, assign_re);
    };

    size_t assign_prefix = 0;
    vector<pair<string, string>> assignments;
    while (assign_prefix < args.size() && is_assignment_word(args[assign_prefix])) {
        const string& tok = args[assign_prefix];
        size_t eq = tok.find('=');
        assignments.push_back({tok.substr(0, eq), tok.substr(eq + 1)});
        ++assign_prefix;
    }

    if (assign_prefix > 0) {
        for (const auto& kv : assignments) {
            bool is_registry_property = kv.first.rfind("HKLM.", 0) == 0 || kv.first.rfind("HKCU.", 0) == 0;
            if (!is_registry_property && !is_valid_env_var_name(kv.first)) {
                cerr << "zsh: invalid variable name: " << kv.first << "\n";
                return 1;
            }
            if (g_env.readonly_vars.count(kv.first)) {
                cerr << "zsh: read-only variable: " << kv.first << "\n";
                return 1;
            }
        }
        if (assign_prefix == args.size()) {
            for (const auto& kv : assignments) {
                string val = assignment_value(kv.second, g_env.integer_vars.count(kv.first) != 0);
                if (kv.first.rfind("HKLM.", 0) == 0 || kv.first.rfind("HKCU.", 0) == 0) {
                    if (!write_zsh_registry_property(kv.first, val)) {
                        cerr << "zsh: failed to set Registry property: " << kv.first << "\n";
                        return 1;
                    }
                } else {
                    g_env.vars[kv.first] = val;
                    SetEnvironmentVariableA(kv.first.c_str(), val.c_str());
                }
            }
            return 0;
        }

        map<string, pair<bool, string>> old_values;
        for (const auto& kv : assignments) {
            auto it = g_env.vars.find(kv.first);
            old_values[kv.first] = {it != g_env.vars.end(), it != g_env.vars.end() ? it->second : ""};
            string val = assignment_value(kv.second, g_env.integer_vars.count(kv.first) != 0);
            g_env.vars[kv.first] = val;
            SetEnvironmentVariableA(kv.first.c_str(), val.c_str());
        }

        vector<string> rest(args.begin() + static_cast<ptrdiff_t>(assign_prefix), args.end());
        int rc = dispatch_command(rest);

        for (const auto& [name, state] : old_values) {
            if (state.first) {
                g_env.vars[name] = state.second;
                SetEnvironmentVariableA(name.c_str(), state.second.c_str());
            } else {
                g_env.vars.erase(name);
                SetEnvironmentVariableA(name.c_str(), nullptr);
            }
        }
        return rc;
    }

    if (args.size() == 1) {
        smatch m;
        static const regex append_list_re(R"(^([A-Za-z_][A-Za-z0-9_]*)\+=\((.*)\)$)");
        static const regex append_one_re(R"(^([A-Za-z_][A-Za-z0-9_]*)\+=(.+)$)");
        if (regex_match(args[0], m, append_list_re)) {
            string name = m[1].str();
            if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
            string payload = trim_copy(m[2].str());
            vector<string> values = payload.empty() ? vector<string>{} : tokenize_words(payload);
            auto& arr = g_env.indexed_arrays[name];
            for (const auto& v : values) {
                string expanded = g_env.expand_vars(v);
                if (!g_env.unique_arrays.count(name) || find(arr.begin(), arr.end(), expanded) == arr.end()) arr.push_back(std::move(expanded));
            }
            return 0;
        }
        if (regex_match(args[0], m, append_one_re)) {
            string name = m[1].str();
            if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
            string value = trim_copy(m[2].str());
            auto& arr = g_env.indexed_arrays[name];
            if (!value.empty()) {
                string expanded = g_env.expand_vars(value);
                if (!g_env.unique_arrays.count(name) || find(arr.begin(), arr.end(), expanded) == arr.end()) arr.push_back(std::move(expanded));
            }
            return 0;
        }
    }

    if (args.size() == 1 && args[0].find('=') != string::npos) {
        size_t eq = args[0].find('=');
        string name = args[0].substr(0, eq);
        bool is_registry_property = name.rfind("HKLM.", 0) == 0 || name.rfind("HKCU.", 0) == 0;
        if (is_registry_property) {
            string value = g_env.expand_vars(args[0].substr(eq + 1));
            if (!write_zsh_registry_property(name, value)) {
                cerr << "zsh: failed to set Registry property: " << name << "\n";
                return 1;
            }
            return 0;
        }
        if (!is_valid_env_var_name(name)) {
            cerr << "zsh: invalid variable name: " << name << "\n";
            return 1;
        }
        if (g_env.readonly_vars.count(name)) {
            cerr << "zsh: read-only variable: " << name << "\n";
            return 1;
        }
        string value = assignment_value(args[0].substr(eq + 1), g_env.integer_vars.count(name) != 0);
        g_env.vars[name] = value;
        return 0;
    }

    if (g_env.aliases.count(args[0])) {
        const string& alias_val = g_env.aliases[args[0]];
        if (contains_dangerous_alias_tokens(alias_val)) {
            cerr << "zsh: unsafe alias definition blocked: " << args[0] << "\n";
            return 1;
        }
        vector<string> exp = tokenize_words(alias_val);
        for (auto& tok : exp) tok = g_env.expand_vars(tok);
        for (size_t i = 1; i < args.size(); ++i) exp.push_back(args[i]);
        args = exp;
    }

    string cmd = args[0];

    if (cmd == "until" || cmd == "repeat" || cmd == "select" || cmd == "time") {
        string block;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i != 0) block += ' ';
            block += args[i];
        }
        if (cmd == "until") return execute_condition_loop(block, true);
        if (cmd == "repeat") return execute_repeat_loop(block);
        if (cmd == "select") return execute_select_loop(block);
        if (cmd == "time") return execute_timed_command(block);
    }

    auto path_for_command = [](const string& name) {
        return find_executable_in_path(name);
    };

    auto print_command_matches = [&](const string& name, bool all, bool kind_only, bool path_only) {
        int matches = 0;
        auto emit = [&](const string& text) {
            cout << text << "\n";
            ++matches;
        };
        if (!path_only && g_env.aliases.count(name)) {
            emit(kind_only ? name + ": alias" : name + ": aliased to " + g_env.aliases[name]);
            if (!all) return 0;
        }
        if (!path_only && g_env.functions.count(name)) {
            emit(kind_only ? name + ": function" : name + " is a shell function");
            if (!all) return 0;
        }
        if (!path_only && is_zsh_builtin_command(name)) {
            emit(kind_only ? name + ": builtin" : name + " is a shell builtin");
            if (!all) return 0;
        }
        string path = path_for_command(name);
        if (!path.empty()) emit(path);
        if (matches == 0) {
            if (!path_only) cout << name << " not found\n";
            return 1;
        }
        return 0;
    };

    if (cmd == "command") {
        size_t start = 1;
        bool path_default = false;
        if (start < args.size() && args[start] == "-p") { path_default = true; ++start; }
        if (start < args.size() && args[start] == "--") ++start;
        if (start >= args.size()) return 0;

        vector<string> pass(args.begin() + static_cast<ptrdiff_t>(start), args.end());
        if (!pass.empty() && (pass[0] == "-v" || pass[0] == "-V")) {
            bool verbose = pass[0] == "-V";
            if (verbose) {
                int rc = 0;
                for (size_t i = 1; i < pass.size(); ++i) {
                    const string& name = pass[i];
                    if (g_env.aliases.count(name)) cout << name << " is an alias for " << g_env.aliases[name] << "\n";
                    else if (g_env.functions.count(name)) cout << name << " is a shell function\n";
                    else if (is_zsh_builtin_command(name)) cout << name << " is a shell builtin\n";
                    else {
                        string path = path_for_command(name);
                        if (!path.empty()) cout << name << " is " << path << "\n";
                        else { cout << name << " not found\n"; rc = 1; }
                    }
                }
                return rc;
            }
            vector<string> q = {"type"};
            for (size_t i = 1; i < pass.size(); ++i) q.push_back(pass[i]);
            return dispatch_command(q);
        }

        const string target = pass[0];
        bool had_alias = g_env.aliases.count(target) != 0;
        bool had_func = g_env.functions.count(target) != 0;
        string saved_alias = had_alias ? g_env.aliases[target] : "";
        string saved_func = had_func ? g_env.functions[target] : "";
        if (had_alias) g_env.aliases.erase(target);
        if (had_func) g_env.functions.erase(target);

        (void)path_default;
        int rc = dispatch_command(pass);

        if (had_alias) g_env.aliases[target] = saved_alias;
        if (had_func) g_env.functions[target] = saved_func;
        return rc;
    }

    if (cmd == "whence") {
        size_t start = 1;
        bool all = false, kind_only = false, path_only = false;
        while (start < args.size() && !args[start].empty() && args[start][0] == '-') {
            for (size_t flag = 1; flag < args[start].size(); ++flag) {
                if (args[start][flag] == 'a') all = true;
                else if (args[start][flag] == 'w') kind_only = true;
                else if (args[start][flag] == 'p') path_only = true;
            }
            ++start;
        }
        if (start >= args.size()) return 0;
        int rc = 0;
        for (size_t i = start; i < args.size(); ++i)
            if (print_command_matches(args[i], all, kind_only, path_only) != 0) rc = 1;
        return rc;
    }

    if (cmd == "nocorrect") {
        if (args.size() == 1) return 0;
        vector<string> rest(args.begin() + 1, args.end());
        return dispatch_command(rest);
    }

    if (cmd == "builtin") {
        if (args.size() < 2) {
            cout << "builtin: usage: builtin <name> [args...]\n";
            return 1;
        }

        vector<string> pass(args.begin() + 1, args.end());
        const string target = pass[0];
        if (!is_zsh_builtin_command(target)) {
            cout << "builtin: " << target << ": not a shell builtin\n";
            return 1;
        }

        bool had_alias = g_env.aliases.count(target) != 0;
        bool had_func = g_env.functions.count(target) != 0;
        string saved_alias = had_alias ? g_env.aliases[target] : "";
        string saved_func = had_func ? g_env.functions[target] : "";
        if (had_alias) g_env.aliases.erase(target);
        if (had_func) g_env.functions.erase(target);
        int rc = dispatch_command(pass);
        if (had_alias) g_env.aliases[target] = saved_alias;
        if (had_func) g_env.functions[target] = saved_func;
        return rc;
    }

    if (g_env.functions.count(cmd)) {
        if (g_function_local_scopes.size() >= static_cast<size_t>(kMaxExecRecursionDepth)) {
            cerr << "zsh: maximum function recursion depth (" << kMaxExecRecursionDepth << ") exceeded\n";
            g_env.last_exit_code = 1;
            return 1;
        }

        vector<string> old_pos = g_env.positional_args;
        bool old_returning = g_script_returning;
        int old_return_code = g_script_return_code;

        g_env.positional_args = vector<string>(args.begin() + 1, args.end());
        g_script_returning = false;
        g_function_local_scopes.push_back({g_env.vars, g_env.indexed_arrays, g_env.assoc_arrays,
                           g_env.integer_vars, g_env.readonly_vars, g_env.unique_arrays, {}});

        int rc = execute_command_line(g_env.functions[cmd]);
        if (g_script_returning) {
            rc = g_script_return_code;
            g_script_returning = false;
        }

        if (g_env.vars.count("__trap_RETURN") && !g_env.vars["__trap_RETURN"].empty()) {
            execute_command_line(g_env.vars["__trap_RETURN"]);
        }

        FunctionLocalScope completed_scope = std::move(g_function_local_scopes.back());
        g_function_local_scopes.pop_back();
        restore_function_locals(completed_scope);

        g_env.positional_args = old_pos;
        g_script_returning = old_returning;
        g_script_return_code = old_return_code;
        return rc;
    }

    if (cmd == "cd" || cmd == "chdir")        return builtin_cd(args);
    if (cmd == "echo")                         return builtin_echo(args);
    if (cmd == "coproc")                       return builtin_coproc(args);
    if (cmd == "read")                         return builtin_read(args);
    if (cmd == "emulate")                      return builtin_emulate(args);
    if (cmd == "local") {
        if (g_function_local_scopes.empty()) { cerr << "local: not in a function\n"; return 1; }
        vector<string> declaration = args;
        declaration[0] = "typeset";
        return builtin_typeset(declaration);
    }
    if (cmd == "readonly") {
        if (args.size() == 1) {
            for (const auto& name : g_env.readonly_vars) {
                cout << "readonly " << name;
                if (g_env.vars.count(name)) cout << "='" << g_env.vars[name] << "'";
                cout << "\n";
            }
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            size_t equal = args[i].find('=');
            string name = equal == string::npos ? args[i] : args[i].substr(0, equal);
            if (!is_valid_env_var_name(name)) { cerr << "readonly: invalid variable name: " << name << "\n"; return 1; }
            if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
            if (equal != string::npos) g_env.vars[name] = args[i].substr(equal + 1);
            else if (!g_env.vars.count(name)) g_env.vars[name] = "";
            g_env.readonly_vars.insert(name);
        }
        return 0;
    }
    if (cmd == "shift") {
        int n = 1;
        if (args.size() > 1) { try { n = stoi(args[1]); } catch (...) { n = 1; } }
        if (n > 0) {
            if (n <= (int)g_env.positional_args.size()) {
                g_env.positional_args.erase(g_env.positional_args.begin(), g_env.positional_args.begin() + n);
            } else {
                g_env.positional_args.clear();
            }
        }
        return 0;
    }
    if (cmd == "typeset" || cmd == "declare") return builtin_typeset(args);
    if (cmd == "jobs")                         return builtin_jobs(args);
    if (cmd == "help")                         return builtin_help(args);
    if (cmd == "pwd")                          { try { cout << normalize_path_to_unix(fs::current_path().string()) << "\n"; } catch (...) { cerr << "pwd: cannot determine current directory\n"; return 1; } return 0; }
    if (cmd == "exit") {
        int code = 0;
        if (args.size() > 1) { try { code = stoi(args[1]); } catch (...) {} }
        if (g_subshell_depth > 0) {
            g_subshell_exiting = true;
            g_subshell_exit_code = code;
            return code;
        }
        exit(code);
    }
    if (cmd == "logout") {
        if (!g_login_shell) {
            cerr << "zsh: logout: not login shell\n";
            return 1;
        }
        int code = 0;
        if (args.size() > 1) { try { code = stoi(args[1]); } catch (...) {} }
        exit(code);
    }

    if (cmd == "export") {
        for (size_t i = 1; i < args.size(); ++i) {
            size_t eq = args[i].find('=');
            if (eq != string::npos) {
                string name = args[i].substr(0, eq), val = args[i].substr(eq + 1);
                if (!is_valid_env_var_name(name)) {
                    cerr << "zsh: invalid variable name: " << name << "\n";
                    return 1;
                }
                g_env.vars[name] = val;
                SetEnvironmentVariableA(name.c_str(), val.c_str());
            } else if (g_env.vars.count(args[i])) {
                if (!is_valid_env_var_name(args[i])) {
                    cerr << "zsh: invalid variable name: " << args[i] << "\n";
                    return 1;
                }
                SetEnvironmentVariableA(args[i].c_str(), g_env.vars[args[i]].c_str());
            }
        }
        return 0;
    }

    if (cmd == "unset") {
        for (size_t i = 1; i < args.size(); ++i) {
            if (!is_valid_env_var_name(args[i])) {
                cerr << "zsh: invalid variable name: " << args[i] << "\n";
                return 1;
            }
            if (g_env.readonly_vars.count(args[i])) {
                cerr << "zsh: read-only variable: " << args[i] << "\n";
                return 1;
            }
            g_env.vars.erase(args[i]);
            g_env.indexed_arrays.erase(args[i]);
            g_env.assoc_arrays.erase(args[i]);
            g_env.integer_vars.erase(args[i]);
            SetEnvironmentVariableA(args[i].c_str(), nullptr);
        }
        return 0;
    }

    if (cmd == "alias") {
        if (args.size() == 1) {
            for (const auto& [k, v] : g_env.aliases) cout << k << "='" << v << "'\n";
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            size_t eq = args[i].find('=');
            if (eq != string::npos) {
                string name = args[i].substr(0, eq);
                string value = args[i].substr(eq + 1);
                if (value.size() >= 2 &&
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\'')))
                    value = value.substr(1, value.size() - 2);
                if (!is_valid_env_var_name(name)) {
                    cerr << "zsh: invalid alias name: " << name << "\n";
                    return 1;
                }
                if (contains_dangerous_alias_tokens(value)) {
                    cerr << "zsh: unsafe alias definition blocked: " << name << "\n";
                    return 1;
                }
                g_env.aliases[name] = value;
            } else if (g_env.aliases.count(args[i])) {
                cout << args[i] << "='" << g_env.aliases[args[i]] << "'\n";
            }
        }
        return 0;
    }

    if (cmd == "unalias") {
        for (size_t i = 1; i < args.size(); ++i) g_env.aliases.erase(args[i]);
        return 0;
    }

    if (cmd == "setopt") {
        if (args.size() == 1) {
            vector<string> names;
            for (const auto& [k, v] : g_env.options) if (v) names.push_back(k);
            sort(names.begin(), names.end());
            for (const auto& n : names) cout << n << "\n";
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            string opt = canonicalize_option_name(args[i]);
            if (opt == "localoptions") {
                if (!g_function_local_scopes.empty() && !g_function_local_scopes.back().local_options) {
                    g_function_local_scopes.back().local_options = true;
                    g_function_local_scopes.back().options_before = g_env.options;
                }
            } else {
                apply_shell_option_token(args[i], true, g_env.options);
            }
        }
        return 0;
    }

    if (cmd == "unsetopt") {
        if (args.size() == 1) {
            vector<string> names;
            for (const auto& [k, v] : g_env.options) if (!v) names.push_back(k);
            sort(names.begin(), names.end());
            for (const auto& n : names) cout << n << "\n";
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) apply_shell_option_token(args[i], false, g_env.options);
        return 0;
    }

    if (cmd == "pushd") {
        string target = args.size() > 1 ? args[1] : g_env.vars["HOME"];
        try { g_env.dir_stack.push_back(fs::current_path().string()); } catch (...) { g_env.dir_stack.push_back(g_env.vars["HOME"]); }
        int rc = builtin_cd({"cd", target});
        if (rc == 0) {
            try { cout << normalize_path_to_unix(fs::current_path().string()); } catch (...) { cout << "?"; }
            for (int j = (int)g_env.dir_stack.size() - 1; j >= 0; --j)
                cout << " " << normalize_path_to_unix(g_env.dir_stack[j]);
            cout << "\n";
        } else {
            g_env.dir_stack.pop_back();
        }
        return rc;
    }

    if (cmd == "popd") {
        if (g_env.dir_stack.empty()) { cout << "popd: directory stack empty\n"; return 1; }
        string prev = g_env.dir_stack.back(); g_env.dir_stack.pop_back();
        return builtin_cd({"cd", prev});
    }

    if (cmd == "dirs") {
        try { cout << normalize_path_to_unix(fs::current_path().string()); } catch (...) { cout << "?"; }
        for (int j = (int)g_env.dir_stack.size() - 1; j >= 0; --j)
            cout << " " << normalize_path_to_unix(g_env.dir_stack[j]);
        cout << "\n";
        return 0;
    }

    if (cmd == "history") {
        if (args.size() > 1 && args[1] == "-c") { g_env.history.clear(); return 0; }
        for (size_t j = 0; j < g_env.history.size(); ++j)
            cout << " " << setw(4) << (j + 1) << "  " << g_env.history[j] << "\n";
        return 0;
    }

    if (cmd == "eval") {
        string expr;
        for (size_t i = 1; i < args.size(); ++i) expr += (i > 1 ? " " : "") + args[i];
        return parse_and_execute(expr);
    }

    if (cmd == "kill") {
        if (args.size() < 2) { cout << "kill: usage: kill <pid> [pid...]\n"; return 1; }
        int rc = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] == '%') {
                size_t job_index = 0;
                if (!resolve_job_index(args[i], job_index)) {
                    cout << "kill: " << args[i] << ": no such job\n";
                    rc = 1;
                    continue;
                }
                BackgroundJob& job = g_env.jobs[job_index];
                BOOL terminated = job.hJob ? TerminateJobObject(job.hJob, 1) : TerminateProcess(job.hProcess, 1);
                if (!terminated) {
                    cout << "kill: " << args[i] << ": unable to terminate job\n";
                    rc = 1;
                }
                continue;
            }
            errno = 0;
            char* end = nullptr;
            unsigned long parsed = strtoul(args[i].c_str(), &end, 10);
            if (errno != 0 || end == args[i].c_str() || *end != '\0' || parsed == 0 || parsed > UINT32_MAX) {
                cout << "kill: " << args[i] << ": invalid pid\n"; rc = 1; continue;
            }
            DWORD pid = (DWORD)parsed;
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (!h || !TerminateProcess(h, 1)) { cout << "kill: " << args[i] << ": no such process\n"; rc = 1; }
            if (h) CloseHandle(h);
        }
        return rc;
    }

    if (cmd == "which" || cmd == "type") {
        if (args.size() < 2) return 1;
        bool all = cmd == "type" && args.size() > 1 && args[1] == "-a";
        size_t start = all ? 2 : 1;
        int rc = 0;
        for (size_t i = start; i < args.size(); ++i)
            if (print_command_matches(args[i], all, false, false) != 0) rc = 1;
        return rc;
    }

    if (cmd == "source" || cmd == ".") {
        size_t index = 1;
        bool trace = false, errexit = false;
        while (index < args.size() && !args[index].empty() && args[index][0] == '-') {
            if (args[index] == "--") { ++index; break; }
            if (args[index] == "-x") trace = true;
            else if (args[index] == "-e") errexit = true;
            else { cerr << cmd << ": unsupported option: " << args[index] << "\n"; return 1; }
            ++index;
        }
        if (index >= args.size()) { cout << cmd << ": filename argument required\n"; return 1; }
        bool has_args = (args.size() > index + 1);
        vector<string> saved_positional = g_env.positional_args;
        if (has_args) {
            g_env.positional_args.assign(args.begin() + static_cast<ptrdiff_t>(index + 1), args.end());
        }
        string old_zero = g_env.vars.count("0") ? g_env.vars["0"] : "zsh";
        g_env.vars["0"] = args[index];
        int rc = execute_script(args[index], trace, errexit);
        g_env.vars["0"] = old_zero;
        if (has_args) {
            g_env.positional_args = std::move(saved_positional);
        }
        return rc;
    }

    if (cmd == "clear" || cmd == "cls") {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(h, &csbi)) {
            DWORD cells = csbi.dwSize.X * csbi.dwSize.Y, written;
            COORD origin = {0, 0};
            FillConsoleOutputCharacterA(h, ' ', cells, origin, &written);
            FillConsoleOutputAttribute(h, csbi.wAttributes, cells, origin, &written);
            SetConsoleCursorPosition(h, origin);
        }
        return 0;
    }

    if (cmd == "dir") {
        // dir is a cmd.exe internal; delegate via cmd /c with any user-supplied args.
        string cmdline = "cmd.exe /c dir";
        for (size_t k = 1; k < args.size(); ++k) {
            string a = args[k];
            bool needs_quote = a.find_first_of(" \t\"") != string::npos;
            if (needs_quote) { string q = "\""; for (char c : a) { if (c=='"') q+="\"\""; else q+=c; } a = q+"\""; }
            cmdline += " " + a;
        }
        wstring wcmd = string_to_wstring(cmdline);
        PROCESS_INFORMATION pi{}; STARTUPINFOW si{}; si.cb = sizeof(si);
        if (!CreateProcessW(NULL, &wcmd[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return 1;
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD ec = 0;
        if (!GetExitCodeProcess(pi.hProcess, &ec)) ec = 1;
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return static_cast<int>(ec);
    }
    if (cmd == "false") return 1;
    if (cmd == "true") return 0;

    if (cmd == "test" || cmd == "[") return builtin_test(args);
    if (cmd == "[[") return builtin_double_bracket(args);
    if (cmd == "print")  return builtin_print(args);
    if (cmd == "printf") return builtin_printf(args);

    if (cmd == "let") {
        int rc = 1;
        for (size_t i = 1; i < args.size(); ++i) {
            string expr = trim_copy(args[i]);
            string var_name, rhs, op_type = "=";
            if (expr.length() >= 2 && expr.substr(expr.length() - 2) == "++") {
                var_name = trim_copy(expr.substr(0, expr.length() - 2));
                op_type = "++";
            } else if (expr.length() >= 2 && expr.substr(expr.length() - 2) == "--") {
                var_name = trim_copy(expr.substr(0, expr.length() - 2));
                op_type = "--";
            } else {
                size_t eq = expr.find('=');
                if (eq != string::npos) {
                    if (eq > 0 && (expr[eq - 1] == '+' || expr[eq - 1] == '-' || expr[eq - 1] == '*' || expr[eq - 1] == '/')) {
                        op_type = string(1, expr[eq - 1]) + "=";
                        var_name = trim_copy(expr.substr(0, eq - 1));
                    } else {
                        string lhs = trim_copy(expr.substr(0, eq));
                        if (!lhs.empty() && (lhs.back() == '+' || lhs.back() == '-' || lhs.back() == '*' || lhs.back() == '/')) {
                            op_type = string(1, lhs.back()) + "=";
                            lhs.pop_back();
                            var_name = trim_copy(lhs);
                        } else {
                            var_name = lhs;
                        }
                    }
                    rhs = trim_copy(expr.substr(eq + 1));
                } else {
                    rc = (eval_math_expr(g_env.expand_vars(expr)) != 0) ? 0 : 1;
                    continue;
                }
            }
            long long cur = 0;
            if (g_env.vars.count(var_name)) { try { cur = stoll(g_env.vars[var_name]); } catch (...) {} }
            long long rv = rhs.empty() ? 0 : eval_math_expr(g_env.expand_vars(rhs));
            long long res;
            if      (op_type == "=")  res = rv;
            else if (op_type == "+=") res = cur + rv;
            else if (op_type == "-=") res = cur - rv;
            else if (op_type == "*=") res = cur * rv;
            else if (op_type == "/=") res = rv ? cur / rv : 0;
            else if (op_type == "++") res = cur + 1;
            else if (op_type == "--") res = cur - 1;
            else res = rv;
            g_env.vars[var_name] = to_string(res);
            rc = (res != 0) ? 0 : 1;
        }
        return rc;
    }

    if (cmd == "break") {
        int count = 1;
        if (args.size() > 1) { try { count = stoi(args[1]); } catch (...) { count = 1; } }
        if (count < 1) count = 1;
        g_loop_breaking = count;
        return 0;
    }

    if (cmd == "continue") {
        int count = 1;
        if (args.size() > 1) { try { count = stoi(args[1]); } catch (...) { count = 1; } }
        if (count < 1) count = 1;
        g_loop_continuing = count;
        return 0;
    }

    if (cmd == "return") {
        g_script_return_code = g_env.last_exit_code;
        if (args.size() > 1) { try { g_script_return_code = stoi(args[1]); } catch (...) {} }
        g_script_returning   = true;
        return g_script_return_code;
    }

    if (cmd == "wait") {
        if (args.size() > 1) {
            size_t idx = 0;
            bool found = false;
            if (!args[1].empty() && args[1][0] == '%') {
                found = resolve_job_index(args[1], idx);
            } else {
                errno = 0;
                char* end = nullptr;
                unsigned long parsed = strtoul(args[1].c_str(), &end, 10);
                if (errno == 0 && end != args[1].c_str() && *end == '\0') {
                    for (size_t i = 0; i < g_env.jobs.size(); ++i) {
                        if (g_env.jobs[i].pid == static_cast<DWORD>(parsed)) { idx = i; found = true; break; }
                    }
                }
            }
            if (!found) { cout << "wait: " << args[1] << ": no such job\n"; return 1; }
            wait_for_background_job(g_env.jobs[idx]);
            close_background_job_handles(g_env.jobs[idx]);
            g_env.jobs.erase(g_env.jobs.begin() + idx);
            repair_job_markers();
        } else {
            for (auto& job : g_env.jobs) {
                wait_for_background_job(job);
                close_background_job_handles(job);
            }
            g_env.jobs.clear();
            repair_job_markers();
        }
        return 0;
    }

    if (cmd == "fg" || cmd == "bg") {
        if (g_env.jobs.empty()) { cout << cmd << ": no current job\n"; return 1; }
        size_t idx = g_env.jobs.size()-1;
        if (args.size()>1 && !resolve_job_index(args[1], idx)) { cout<<cmd<<": no such job\n"; return 1; }
        auto& job = g_env.jobs[idx];
        refresh_background_job_state(job);
        if (job.state == JobState::Done || job.state == JobState::Failed) {
            cout<<"["<<job.job_id<<"] "<<job_state_name(job.state)<<"  "<<job.command<<"\n";
            close_background_job_handles(job);
            g_env.jobs.erase(g_env.jobs.begin()+idx);
            repair_job_markers();
            return 0;
        }
        if (cmd=="fg") {
            if (job.state == JobState::Stopped) {
                bool resumed = true;
                for (DWORD pid : job.pids) resumed = resume_win32_process(pid) && resumed;
                if (!resumed) { cerr << "fg: failed to resume job\n"; return 1; }
                job.state = JobState::Running;
            }
            cout << job.command << "\n";
            set_foreground_pids(job.pids);
            struct ForegroundPidsScopeGuard {
                ~ForegroundPidsScopeGuard() { clear_foreground_pids(); }
            } fg_scope_guard;
            wait_for_background_job(job);
            DWORD ec=0; GetExitCodeProcess(job.hProcess, &ec);
            close_background_job_handles(job);
            g_env.jobs.erase(g_env.jobs.begin()+idx);
            repair_job_markers();
            return (int)ec;
        }
        bool resumed = true;
        if (job.state == JobState::Stopped) {
            for (DWORD pid : job.pids) resumed = resume_win32_process(pid) && resumed;
        }
        if (!resumed) { cerr << "bg: failed to resume job\n"; return 1; }
        job.state = JobState::Running;
        select_current_job(job.job_id);
        cout << "[" << job.job_id << "] " << job.command << " &\n";
        return 0;
    }

    if (cmd == "disown") {
        if (g_env.jobs.empty()) return 0;
        size_t idx = g_env.jobs.size()-1;
        if (args.size()>1 && !resolve_job_index(args[1], idx)) { cout << "disown: no such job\n"; return 1; }
        close_background_job_handles(g_env.jobs[idx]);
        g_env.jobs.erase(g_env.jobs.begin()+idx);
        repair_job_markers();
        return 0;
    }

    if (cmd == "fc") {
        bool list = args.size()>1 && args[1]=="-l";
        if (list) {
            size_t s = g_env.history.size()>16 ? g_env.history.size()-16 : 0;
            for (size_t j=s; j<g_env.history.size(); ++j)
                cout << " " << setw(4) << (j + 1) << "  " << g_env.history[j] << "\n";
        } else if (args.size()>1) {
            errno = 0; char* end = nullptr;
            unsigned long n = strtoul(args[1].c_str(), &end, 10);
            if (errno == 0 && end != args[1].c_str() && *end == '\0' && n>=1 && n<=g_env.history.size()) {
                string s = g_env.history[n-1]; cout << s << "\n";
                return parse_and_execute(s);
            }
        }
        return 0;
    }

    if (cmd == "integer") {
        for (size_t i=1; i<args.size(); ++i) {
            size_t eq=args[i].find('=');
            const string name = eq == string::npos ? args[i] : args[i].substr(0, eq);
            if (!is_valid_env_var_name(name)) {
                cerr << "integer: invalid variable name: " << name << "\n";
                return 1;
            }
            if (g_env.readonly_vars.count(name)) {
                cerr << "zsh: read-only variable: " << name << "\n";
                return 1;
            }
            mark_function_local(name);
            g_env.integer_vars.insert(name);
            if (eq != string::npos) {
                string value = g_env.expand_vars(args[i].substr(eq + 1));
                g_env.vars[name] = to_string(eval_math_expr(value));
            } else if (!g_env.vars.count(name)) {
                g_env.vars[name] = "0";
            }
        }
        return 0;
    }

    if (cmd == "getopts") {
        if (args.size()<3) return 1;
        const string& optstr=args[1]; const string& varname=args[2];
        int optind=1;
        if (g_env.vars.count("OPTIND")) { try{optind=stoi(g_env.vars["OPTIND"]);}catch(...){} }
        vector<string> pargs = args.size()>3 ? vector<string>(args.begin()+3,args.end()) : g_env.positional_args;
        int offset = 1;
        if (g_env.vars.count("__getopts_offset")) { try { offset = stoi(g_env.vars["__getopts_offset"]); } catch (...) {} }
        int tracked_optind = -1;
        if (g_env.vars.count("__getopts_optind")) { try { tracked_optind = stoi(g_env.vars["__getopts_optind"]); } catch (...) {} }
        if (tracked_optind != optind || !g_env.vars.count("__getopts_spec") || g_env.vars["__getopts_spec"] != optstr) offset = 1;
        if (optind>(int)pargs.size()) { g_env.vars[varname]="?"; return 1; }
        const string& arg=pargs[optind-1];
        if (arg.empty()||arg[0]!='-'||arg=="--") { g_env.vars[varname]="?"; return 1; }
        if (offset >= static_cast<int>(arg.size())) { ++optind; offset = 1; }
        if (optind > static_cast<int>(pargs.size())) { g_env.vars[varname]="?"; return 1; }
        const string& current = pargs[optind-1];
        char opt=current[static_cast<size_t>(offset++)];
        bool silent = !optstr.empty() && optstr[0] == ':';
        size_t pos=optstr.find(opt);
        g_env.vars.erase("OPTARG");
        if (pos==string::npos || (silent && pos == 0)) {
            g_env.vars[varname]="?";
            g_env.vars["OPTARG"] = string(1, opt);
            if (!silent) cerr<<"zsh: illegal option: -"<<opt<<"\n";
        } else {
            g_env.vars[varname]=string(1,opt);
            if (pos+1<optstr.size()&&optstr[pos+1]==':') {
                if (offset < static_cast<int>(current.size())) {
                    g_env.vars["OPTARG"]=current.substr(static_cast<size_t>(offset));
                    offset = static_cast<int>(current.size());
                } else if (optind<(int)pargs.size()) {
                    ++optind; g_env.vars["OPTARG"]=pargs[optind-1];
                } else {
                    g_env.vars[varname]=silent ? ":" : "?";
                    g_env.vars["OPTARG"] = string(1, opt);
                    if (!silent) cerr<<"zsh: option requires argument: -"<<opt<<"\n";
                }
            }
        }
        if (offset >= static_cast<int>(current.size())) { ++optind; offset = 1; }
        g_env.vars["OPTIND"]=to_string(optind);
        g_env.vars["__getopts_offset"]=to_string(offset);
        g_env.vars["__getopts_optind"]=to_string(optind);
        g_env.vars["__getopts_spec"]=optstr;
        return 0;
    }

    if (cmd == "set") {
        if (args.size() == 1) {
            for (const auto& [k, v] : g_env.vars) {
                if (k.rfind("__", 0) != 0) cout << k << "=" << v << "\n";
            }
            return 0;
        }
        if (args.size() >= 2 && args[1] == "--") {
            g_env.positional_args.assign(args.begin() + 2, args.end());
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            const string& arg = args[i];
            if (arg == "-x") g_env.options["xtrace"] = true;
            else if (arg == "+x") g_env.options["xtrace"] = false;
            else if (arg == "-e") g_env.options["errexit"] = true;
            else if (arg == "+e") g_env.options["errexit"] = false;
            else if (arg == "-u") g_env.options["nounset"] = true;
            else if (arg == "+u") g_env.options["nounset"] = false;
            else if (arg == "--") {
                g_env.positional_args.assign(args.begin() + i + 1, args.end());
                break;
            } else if (arg.size() > 1 && (arg[0] == '-' || arg[0] == '+')) {
                apply_shell_option_token(arg.substr(1), arg[0] == '-', g_env.options);
            } else {
                g_env.positional_args.assign(args.begin() + i, args.end());
                break;
            }
        }
        return 0;
    }

    if (cmd == "trap") {
        if (args.size() == 1) {
            for (const auto& [k, v] : g_env.vars) {
                if (k.rfind("__trap_", 0) == 0) cout << "trap -- '" << v << "' " << k.substr(7) << "\n";
            }
            return 0;
        }
        if (args.size() == 2 && args[1] == "-") {
            vector<string> to_erase;
            for (const auto& [k, v] : g_env.vars) {
                if (k.rfind("__trap_", 0) == 0) to_erase.push_back(k);
            }
            for (const auto& k : to_erase) g_env.vars.erase(k);
            return 0;
        }
        if (args.size() >= 3) {
            string handler = args[1];
            for (size_t k = 2; k < args.size(); ++k) {
                string sig = args[k];
                if (sig.rfind("SIG", 0) == 0) sig = sig.substr(3);
                if (sig == "0") sig = "EXIT";
                else if (sig == "1") sig = "HUP";
                else if (sig == "2") sig = "INT";
                else if (sig == "3") sig = "QUIT";
                else if (sig == "15") sig = "TERM";
                else if (sig == "20") sig = "TSTP";
                else if (sig == "28") sig = "WINCH";
                if (handler == "-" || handler.empty()) {
                    g_env.vars.erase("__trap_" + sig);
                } else {
                    g_env.vars["__trap_" + sig] = handler;
                }
            }
            return 0;
        }
        return 0;
    }

    if (cmd == "times") {
        FILETIME cr,ex,ku,us;
        if (GetProcessTimes(GetCurrentProcess(),&cr,&ex,&ku,&us)) {
            auto s=[](FILETIME f)->double{ ULARGE_INTEGER t; t.LowPart=f.dwLowDateTime; t.HighPart=f.dwHighDateTime; return t.QuadPart/10000000.0; };
            ostringstream tout;
            tout << fixed << setprecision(3) << s(us) << "s " << s(ku) << "s\n0.000s 0.000s\n";
            cout << tout.str();
        }
        return 0;
    }

    if (cmd == "autoload") {
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] != '-') g_env.autoload_functions.insert(args[i]);
        }
        return 0;
    }

    if (cmd == "compinit") {
        vector<string> search_paths;
        if (g_env.indexed_arrays.count("fpath")) search_paths = g_env.indexed_arrays["fpath"];
        if (g_env.vars.count("FPATH")) {
            stringstream paths(g_env.vars["FPATH"]);
            string path;
            while (getline(paths, path, ';')) if (!path.empty()) search_paths.push_back(path);
        }
        for (const auto& path : search_paths) {
            error_code ec;
            for (fs::directory_iterator it(normalize_path_to_win(path), ec), end; !ec && it != end; it.increment(ec)) {
                string name = it->path().filename().string();
                if (name.size() > 1 && name[0] == '_') {
                    string command_name = name.substr(1);
                    g_env.completion_definitions[command_name] = name;
                    g_env.autoload_functions.insert(name);
                }
            }
        }
        return 0;
    }

    if (cmd == "compdef") {
        if (args.size() == 1) {
            for (const auto& [command_name, function_name] : g_env.completion_definitions)
                cout << "compdef " << function_name << " " << command_name << "\n";
            return 0;
        }
        if (args[1] == "-d") {
            for (size_t i = 2; i < args.size(); ++i) g_env.completion_definitions.erase(args[i]);
            return 0;
        }
        if (args.size() < 3) { cerr << "compdef: usage: compdef function command...\n"; return 1; }
        for (size_t i = 2; i < args.size(); ++i) g_env.completion_definitions[args[i]] = args[1];
        return 0;
    }

    if (cmd == "compadd") {
        auto& reply = g_env.indexed_arrays["reply"];
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] == '-') continue;
            reply.push_back(args[i]);
        }
        return 0;
    }

    if (cmd == "bindkey") {
        if (args.size() == 1 || (args.size() == 2 && args[1] == "-L")) {
            for (const auto& [keymap, bindings] : g_env.keymaps)
                for (const auto& [key, widget] : bindings)
                    cout << "bindkey -M '" << keymap << "' '" << key << "' '" << widget << "'\n";
            return 0;
        }
        if (args.size() == 4 && args[1] == "-A") {
            auto found = g_env.keymaps.find(args[2]);
            if (found == g_env.keymaps.end()) { cerr << "bindkey: no such keymap: " << args[2] << "\n"; return 1; }
            g_env.keymaps[args[3]] = found->second;
            return 0;
        }
        if (args.size() >= 5 && args[1] == "-M" && args[3] == "-r") {
            g_env.keymaps[args[2]].erase(args[4]);
            return 0;
        }
        if (args.size() >= 5 && args[1] == "-M") {
            g_env.keymaps[args[2]][args[3]] = args[4];
            return 0;
        }
        cerr << "bindkey: unsupported arguments\n";
        return 1;
    }

    if (cmd == "zstyle") {
        if (args.size() == 2 && args[1] == "-L") {
            for (const auto& [key, values] : g_env.styles) {
                cout << "zstyle '" << key.first << "' '" << key.second << "'";
                for (const auto& value : values) cout << " '" << value << "'";
                cout << "\n";
            }
            return 0;
        }
        if (args.size() >= 3 && args[1] == "-d") {
            if (args.size() == 3) {
                for (auto it = g_env.styles.begin(); it != g_env.styles.end(); ) {
                    if (it->first.first == args[2]) it = g_env.styles.erase(it); else ++it;
                }
            } else {
                g_env.styles.erase({args[2], args[3]});
            }
            return 0;
        }
        if (args.size() < 4) { cerr << "zstyle: usage: zstyle context style value...\n"; return 1; }
        g_env.styles[{args[1], args[2]}] = vector<string>(args.begin() + 3, args.end());
        return 0;
    }

    if (cmd == "zle") {
        if (args.size() == 2 && args[1] == "-l") {
            for (const auto& [name, function_name] : g_env.widgets) cout << name << "\n";
            return 0;
        }
        if (args.size() >= 3 && args[1] == "-D") {
            for (size_t i = 2; i < args.size(); ++i) g_env.widgets.erase(args[i]);
            return 0;
        }
        if (args.size() == 4 && args[1] == "-A") {
            auto found = g_env.widgets.find(args[2]);
            if (found == g_env.widgets.end()) { cerr << "zle: no such widget: " << args[2] << "\n"; return 1; }
            g_env.widgets[args[3]] = found->second;
            return 0;
        }
        if (args.size() >= 3 && args[1] == "-N") {
            g_env.widgets[args[2]] = args.size() > 3 ? args[3] : args[2];
            return 0;
        }
        cerr << "zle: unsupported arguments\n";
        return 1;
    }

    if (cmd == "zmodload") {
        if (args.size() == 1) {
            for (const auto& module : g_env.loaded_modules) cout << module << "\n";
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] == '-') continue;
            g_env.loaded_modules.insert(args[i]);
        }
        if (g_env.loaded_modules.count("zsh/datetime")) {
            auto now = chrono::system_clock::now();
            auto seconds = chrono::duration_cast<chrono::seconds>(now.time_since_epoch());
            auto micros = chrono::duration_cast<chrono::microseconds>(now.time_since_epoch());
            g_env.vars["EPOCHSECONDS"] = to_string(seconds.count());
            ostringstream realtime;
            realtime << seconds.count() << '.' << setw(6) << setfill('0') << (micros.count() % 1000000);
            g_env.vars["EPOCHREALTIME"] = realtime.str();
        }
        if (g_env.loaded_modules.count("zsh/system")) g_env.vars["SYS_PID"] = to_string(GetCurrentProcessId());
        if (g_env.loaded_modules.count("zsh/parameter")) {
            auto& parameters = g_env.assoc_arrays["parameters"];
            parameters.clear();
            for (const auto& [name, value] : g_env.vars) parameters[name] = "scalar";
            for (const auto& [name, value] : g_env.indexed_arrays) parameters[name] = "array";
            for (const auto& [name, value] : g_env.assoc_arrays)
                if (name != "parameters") parameters[name] = "association";
            auto& commands = g_env.assoc_arrays["commands"];
            commands.clear();
            for (const auto& name : zsh_builtin_command_names()) commands[name] = "builtin";
            auto& functions = g_env.assoc_arrays["functions"];
            functions.clear();
            for (const auto& [name, body] : g_env.functions) functions[name] = body;
            auto& aliases = g_env.assoc_arrays["aliases"];
            aliases = g_env.aliases;
        }
        return 0;
    }

    if (cmd == "strftime") {
        if (!g_env.loaded_modules.count("zsh/datetime")) { cerr << "strftime: zsh/datetime is not loaded\n"; return 1; }
        string format = args.size() > 1 ? args[1] : "%c";
        time_t timestamp = time(nullptr);
        if (args.size() > 2) {
            try { timestamp = static_cast<time_t>(stoll(args[2])); } catch (...) { return 1; }
        }
        tm local_time{};
        if (localtime_s(&local_time, &timestamp) != 0) return 1;
        char result[1024] = {};
        if (::strftime(result, sizeof(result), format.c_str(), &local_time) == 0) return 1;
        cout << result << "\n";
        return 0;
    }

    if (cmd == "sysread") {
        if (!g_env.loaded_modules.count("zsh/system")) { cerr << "sysread: zsh/system is not loaded\n"; return 1; }
        int fd = 0;
        size_t argument = 1;
        if (args.size() > 2 && args[1] == "-i") { try { fd = stoi(args[2]); } catch (...) { return 1; } argument = 3; }
        string variable = argument < args.size() ? args[argument] : "REPLY";
        HANDLE handle = shell_fd_handle(fd);
        if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return 1;
        string value;
        char character = 0;
        DWORD read = 0;
        while (ReadFile(handle, &character, 1, &read, nullptr) && read == 1 && character != '\n') {
            if (character != '\r') value += character;
        }
        if (value.empty() && read == 0) return 1;
        g_env.vars[variable] = value;
        g_env.vars["REPLY"] = value;
        return 0;
    }

    if (cmd == "syswrite") {
        if (!g_env.loaded_modules.count("zsh/system")) { cerr << "syswrite: zsh/system is not loaded\n"; return 1; }
        int fd = 1;
        size_t argument = 1;
        if (args.size() > 2 && args[1] == "-o") { try { fd = stoi(args[2]); } catch (...) { return 1; } argument = 3; }
        string payload;
        for (size_t i = argument; i < args.size(); ++i) {
            if (!payload.empty()) payload += ' ';
            payload += args[i];
        }
        HANDLE handle = shell_fd_handle(fd);
        if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return 1;
        DWORD written = 0;
        return WriteFile(handle, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr) &&
               written == payload.size() ? 0 : 1;
    }

    if (cmd == "rehash") {
        unsigned long long epoch = 0;
        if (g_env.vars.count("__command_path_epoch")) {
            try { epoch = stoull(g_env.vars["__command_path_epoch"]); } catch (...) {}
        }
        g_env.vars["__command_path_epoch"] = to_string(epoch + 1);
        return 0;
    }

    error_code _autocd_ec;
    string _win_cmd = normalize_path_to_win(cmd);
    bool is_drive_letter = (cmd.size() == 2 && isalpha((unsigned char)cmd[0]) && cmd[1] == ':');
    if (args.size() == 1 && g_env.options.count("autocd") && g_env.options.at("autocd") &&
        (is_drive_letter || fs::is_directory(_win_cmd, _autocd_ec))) {
        return builtin_cd({"cd", cmd});
    }

    Pipeline pl; SingleCmd sc; sc.args = args; pl.cmds.push_back(sc);
    return execute_pipeline_native(pl);
}
