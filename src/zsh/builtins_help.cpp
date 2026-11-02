#include "builtins.hpp"

using namespace std;

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
  where <cmd>                List all occurrences of cmd in aliases, builtins, and PATH.
  command <cmd> [args..]     Execute while bypassing alias/function wrappers.
  builtin <name> [args..]    Invoke a shell builtin directly.
  hash [-r] [-d] [name=path] Display, flush, or modify command or directory hash table.

  BUILTIN & OPTION MANAGEMENT
  disable [-a|-f] [name...]  Disable builtins, aliases (-a), or functions (-f).
  enable [-a|-f] [name...]   Re-enable builtins, aliases (-a), or functions (-f).
  limit [-h] [res [val]]     Display or set named resource limits.
  unlimit [-h] [res...]      Remove resource limits (set to unlimited).
  ulimit [-a] [-opt [val]]   POSIX/KornShell resource limit query and configuration.

  INTERACTIVE & SCHEDULING
  vared [-p prompt] <var>    Interactively edit variable or array in-place.
  suspend [-f]               Suspend shell execution until a key is pressed.
  sched [+]time <cmd>        Schedule command for deferred execution.
  stat [flags] <file...>     Display low-level file status attributes.
  echotc / echoti <cap>      Emit terminal capability control codes.

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

static const char* kZshHelpWhereBody = R"(
  where <name>...
    Search for all occurrences of each <name> in resolution order:
      1. Shell aliases
      2. Shell functions
      3. Shell built-in commands
      4. Executables found across all directories in PATH

  Examples:
    where ls
    where cd git
)";

static const char* kZshHelpHashBody = R"(
  hash [-r] [-v] [-d] [name[=path]...]
    Inspect, populate, or clear the shell command or directory hash table.

  Options:
    -r           Flush the hash table (rehash).
    -v           Verbose progress output.
    -d           Operate on the named-directory hash table (~name).
    name=path    Explicitly associate name with target path.
    name         Find and hash name from PATH.

  Examples:
    hash -r                  # Flush command hash table
    hash git=C:/bin/git.exe  # Map command git
    hash -d proj=C:/Work     # Map named directory ~proj
)";

static const char* kZshHelpDisableEnableBody = R"(
  disable [-a|-f] [name...]
  enable  [-a|-f] [name...]
    Disable or re-enable shell built-in commands, aliases, or functions.

  Options:
    -a           Operate on aliases.
    -f           Operate on functions.
    (default)    Operate on built-in commands.

  Examples:
    disable echo             # Disable built-in echo (falls through to external echo.exe)
    enable echo              # Re-enable built-in echo
    disable -a ll            # Disable alias ll
    disable                  # List all disabled built-ins
)";

static const char* kZshHelpLimitsBody = R"(
  limit [-h] [resource [value]]
  unlimit [-h] [resource...]
  ulimit [-a] [-HS] [-c|-d|-f|-l|-m|-n|-s|-t|-u|-v] [value]
    Display or configure process and job resource limits.

  Resources (limit/unlimit):
    cputime, filesize, datasize, stacksize, coredumpsize,
    memoryuse, maxproc, descriptors, memorylocked, addressspace.

  Options (ulimit):
    -a           Display all resource limits.
    -n [N]       File descriptors limit.
    -u [N]       Maximum processes limit.
    -t [N]       CPU time limit (seconds).
    -f [N]       File size limit (blocks).

  Examples:
    limit                    # Display all named resource limits
    limit descriptors 4096   # Set open file descriptors limit
    unlimit cputime          # Remove CPU time restriction
    ulimit -a                # Display POSIX resource limits
)";

static const char* kZshHelpVaredBody = R"(
  vared [-p prompt] [-c] <var>
    Interactively edit a variable or array value in-place using the terminal editor.

  Options:
    -p prompt    Display a custom prompt before editing.
    -c           Create variable if it does not already exist.

  Examples:
    vared PATH
    vared -p "Edit compiler: " CC
)";

static const char* kZshHelpSchedBody = R"(
  sched [+]time command...
  sched -<item>
  sched
    Schedule shell commands for deferred execution.

  Usage:
    sched                List all scheduled events.
    sched +seconds cmd   Run cmd after a delay of seconds.
    sched +hh:mm cmd     Run cmd after hh hours and mm minutes.
    sched hh:mm cmd      Run cmd at specified clock time.
    sched -N             Cancel scheduled event number N.

  Examples:
    sched +60 print "1 minute has elapsed"
    sched +01:30 echo "Meeting time"
    sched -1
)";

static const char* kZshHelpStatBody = R"(
  stat [-s] [-L] [-l] [-A array] [+field] files...
    Display low-level file status and metadata without external utilities.

  Options:
    -s           Silent test mode (exit status 0 if file exists, 1 otherwise).
    -L           Follow symbolic links and reparse points.
    -l           Display long multi-line labeled attribute list.
    -A assoc     Save file attributes into named associative array.
    +field       Print single field: size, mtime, atime, ctime, mode, type, blocks.

  Examples:
    stat README.md
    stat +size config.json
    stat -A info build.log && echo "Size: $info[size]"
)";

static const char* kZshHelpEchotcBody = R"(
  echotc <cap> [args...]
  echoti <cap> [args...]
    Emit terminal capabilities and control sequences.

  Capabilities:
    clear (cl), home (ho), up (cuu1), down (cud1), right (cuf1), left (cub1),
    bold (md), dim (mh), smul (us), rmul (ue), rev (mr), sgr0 (me),
    ed (cd), el (ce), bell (bl), lines (li), cols (co), cup <row> <col>.

  Examples:
    echotc bold; print "Bold text"; echotc sgr0
    echotc clear
    echotc cols
)";

static const char* kZshHelpManualSeparator = "========================================================================\n";
static const char* kZshHelpManualTitle = "CrossShellZSH v3.7.18\n";
static const char* kZshHelpManualSubtitle = "General Help\n";

static bool print_zsh_help_topic(const string& topic) {
    static const ZshHelpTopicData topics[] = {
        {"builtins", &COLOR_BR_GREEN, "\n=== CrossShellZSH BUILT-IN COMMANDS ===", kZshHelpBuiltinsBody},
        {"test", &COLOR_BR_CYAN, "\n=== TEST / [ EXPRESSIONS ===", kZshHelpTestBody},
        {"arrays", &COLOR_BR_YELLOW, "\n=== ZSH ARRAYS & VARIABLES ===", kZshHelpArraysBody},
        {"globbing", &COLOR_BR_CYAN, "\n=== EXTENDED GLOBBING & QUALIFIERS ===", kZshHelpGlobbingBody},
        {"piping", &COLOR_BR_BLUE, "\n=== PIPELINES, REDIRECTION & JOBS ===", kZshHelpPipingBody},
        {"where", &COLOR_BR_GREEN, "\n=== WHERE COMMAND ===", kZshHelpWhereBody},
        {"hash", &COLOR_BR_GREEN, "\n=== HASH COMMAND ===", kZshHelpHashBody},
        {"disable", &COLOR_BR_GREEN, "\n=== DISABLE / ENABLE COMMANDS ===", kZshHelpDisableEnableBody},
        {"enable", &COLOR_BR_GREEN, "\n=== DISABLE / ENABLE COMMANDS ===", kZshHelpDisableEnableBody},
        {"limit", &COLOR_BR_GREEN, "\n=== LIMIT / ULIMIT COMMANDS ===", kZshHelpLimitsBody},
        {"ulimit", &COLOR_BR_GREEN, "\n=== LIMIT / ULIMIT COMMANDS ===", kZshHelpLimitsBody},
        {"unlimit", &COLOR_BR_GREEN, "\n=== LIMIT / ULIMIT COMMANDS ===", kZshHelpLimitsBody},
        {"vared", &COLOR_BR_GREEN, "\n=== VARED COMMAND ===", kZshHelpVaredBody},
        {"sched", &COLOR_BR_GREEN, "\n=== SCHED COMMAND ===", kZshHelpSchedBody},
        {"stat", &COLOR_BR_GREEN, "\n=== STAT COMMAND ===", kZshHelpStatBody},
        {"echotc", &COLOR_BR_GREEN, "\n=== ECHOTC / ECHOTI COMMANDS ===", kZshHelpEchotcBody},
        {"echoti", &COLOR_BR_GREEN, "\n=== ECHOTC / ECHOTI COMMANDS ===", kZshHelpEchotcBody}
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
