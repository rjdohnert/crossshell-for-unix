# CrossShellKSH

**CrossShellKSH** is a high-performance, standalone native Windows implementation of the **AT&T KornShell (ksh93)** command language interpreter. Built from the ground up in C++ as a single-file zero-dependency engine, it provides full interactive shell capabilities, advanced scripting, and deep integration with Microsoft Windows and Microsoft Windows Server systems without requiring POSIX emulation layers like Cygwin or MSYS2.

---

## Table of Contents

1. [Architectural Overview & Core Philosophy](#1-architectural-overview--core-philosophy)
2. [Key Strengths](#2-key-strengths)
3. [Differences from Standard AT&T ksh93](#3-differences-from-standard-att-ksh93)
4. [Shell Syntax & Language Features](#4-shell-syntax--language-features)
5. [Complete Built-in Command Reference](#5-complete-built-in-command-reference)
6. [Interactive Line Editing (Vi Mode)](#6-interactive-line-editing-vi-mode)
7. [Prompt Customization & Startup Profile (`.kshrc`)](#7-prompt-customization--startup-profile-kshrc)
8. [Windows Registry Integration](#8-windows-registry-integration)
9. [Scripting Walkthrough & Practical Examples](#9-scripting-walkthrough--practical-examples)
10. [Building & Compiling](#10-building--compiling)

---

## 1. Architectural Overview & Core Philosophy

CrossShellKSH is designed specifically for Windows environments that demand the power and expressiveness of KornShell with the speed, portability, and native integration of Win32 executables.

- **Zero External Dependencies**: Compiles directly against core Windows system DLLs (`KERNEL32.dll`, `ADVAPI32.dll`, `SHELL32.dll`, `USER32.dll`) No POSIX emulation layer, Cygwin runtime, or external package managers are needed.
- **Microsecond Startup Latency**: Boots in single-digit milliseconds (~5 ms), avoiding the process startup overhead of virtual machines, WSL, or translation runtimes.
- **Hardened Execution Model**: Guarded against runaway recursion, stack overflow, memory exhaustion, and deep subshell nesting through RAII depth monitors and resource caps.

---

## 2. Key Strengths

### 🚀 Native Win32 Performance & Portability
- Runs directly as a standalone binary (`bin\ksh.exe`).
- Requires no installation or complex path mounting—drop it into any directory or USB drive and run.

### 🧩 Rich ksh93 Compatibility Surface
- **Compound & Custom Struct Types (`typeset -T`)**: Define hierarchical data types with typed fields and default initializers.
- **Associative & Indexed Arrays**: Full support for `typeset -A`, `set -A`, and sparse array assignments (`arr=([key]=val)`).
- **First-Class Co-Processes (`coproc` / `|&`)**: Bidirectional process wiring via `print -p`, `read -p`, and `wait -p`.
- **Advanced Math Engine**: Complete `$(( ... ))` arithmetic supporting 64-bit integers, floating-point decimals, and math functions (`sin`, `cos`, `tan`, `sqrt`, `pow`, `abs`, `log`, `exp`, `floor`, `ceil`, `round`, `min`, `max`).
- **Interactive Menu Flow (`select`)**: Built-in menu generation with `$PS3` prompt and `$REPLY` processing.
- **Extended Pattern Matching**: Full support for ksh93 globbing constructs (`*(pattern)`, `+(pattern)`, `?(pattern)`, `@(pattern)`, `!(pattern)`).

### 🖥️ Modern Terminal & Console Integration
- **Full UTF-8 Code Page**: Default output and input code pages are configured to `CP_UTF8`.
- **ANSI / Virtual Terminal Processing**: Native support for ANSI 24-bit RGB colors, cursor positioning, and Nerd Font / Powerline glyphs.
- **Modal Vi Line Editor**: Complete Vi editing model with normal/insert states, character find motions, count prefixes, and history searching.

### 🛡️ Hardened Resource Security & Depth Guards
- Function recursion depth capped at `200`.
- Expansion and evaluation nesting depth capped at `64`.
- Arithmetic parser depth capped at `200`.
- Pattern matching step budgets capped at `200,000` steps to prevent catastrophic regex/glob backtracking.

---

## 3. Differences from Standard AT&T ksh93

While CrossShellKSH maintains strict syntactic compatibility with ksh93, several key architectural differences exist due to the underlying Windows operating system:

| Feature / Domain | AT&T ksh93 (Unix / POSIX) | CrossShellKSH (Windows Native) |
| :--- | :--- | :--- |
| **Process Model** | `fork()` and `exec()` | Native Win32 `CreateProcessW` and in-process threads |
| **Path Formats** | Forward slashes only (`/usr/bin`) | Forward slashes (`/`), backslashes (`\`), drive letters (`C:\`), and UNC paths (`\\server\share`) |
| **Windows Registry** | Not applicable | First-class variable namespace (`${HKLM.Software...}`) |
| **Signals & Traps** | POSIX signals (`SIGINT`, `SIGTERM`, `SIGHUP`, `SIGCHLD`, etc.) | Windows console control events (`CTRL_C_EVENT`, `CTRL_BREAK_EVENT`) and simulated traps |
| **Process Substitution** | Unix FIFOs / Named Pipes (`/dev/fd/N`) | Windows Named Pipes (`\\.\pipe\ksh_proc_sub_*`) with background relay threads |
| **File Descriptors** | Standard numeric descriptors $0..N$ via POSIX VFS | File handles ($0, 1, 2$) mapped to standard Win32 handles; descriptors $3..9$ supported via `exec` |
| **Executable Lookup** | `PATH` lookup with execute bit verification (`chmod +x`) | Windows `SearchPathW` checking `.exe`, `.cmd`, `.bat`, `.ksh`, and executable file extensions |
| **Job Control & Suspend** | POSIX job control with `SIGCONT` / `SIGSTOP` | Windows process handles; background tasks run continuously |

---

## 4. Shell Syntax & Language Features

### Pipelines and Job Control
```ksh
# Pipeline standard output into the next command
cat file.txt | grep "pattern" | wc -l

# Run command in the background
sleep 5 &

# Co-process pipeline launch
coproc python worker.py
print -p "ping"
read -p response
print "Worker replied: $response"
```

### Process Substitution
```ksh
# Diff output of two dynamic commands using Windows named-pipe relays
diff <(sort list1.txt) <(sort list2.txt)
```

### Variables and Expansions
```ksh
# Scalar assignment
name="Antigravity"
print "Hello, ${name}!"

# Substring extraction: ${var:offset:length}
print "${name:0:4}"       # Anti

# Pattern replacement: ${var/pattern/replacement}
path="C:/Users/Admin/Docs"
print "${path//\//\\}"    # C:\Users\Admin\Docs

# Default values
print "${UNDEFINED:-default_val}"
```

### Indexed & Associative Arrays
```ksh
# Indexed array
typeset -a fruits
fruits[0]="apple"
fruits[1]="banana"
print "Fruits: ${fruits[@]} (count: ${#fruits[@]})"

# Associative array
typeset -A capitals
capitals[France]="Paris"
capitals[Germany]="Berlin"
capitals[Japan]="Tokyo"
print "Capital of Japan is: ${capitals[Japan]}"
print "All keys: ${!capitals[@]}"
```

### Compound Data Types (`typeset -T`)
```ksh
# Define a custom struct type
typeset -T Server_t=(
    typeset host="localhost"
    integer port=8080
    typeset active="true"
)

# Instantiate instances
Server_t web_server=(host="192.168.1.10" port=443)
print "Server: ${web_server.host}:${web_server.port}"
```

### Math & Arithmetic Evaluation
```ksh
# Integer and floating-point math
result=$(( 10 * 3.5 + sqrt(144) - (2 ** 4) ))
print "Result: $result"

# Bitwise and ternary operators
print "$(( 5 ^ 3 ))"                 # Bitwise XOR (6)
print "$(( 10 > 5 ? 100 : 200 ))"    # Ternary (100)
```

---

## 5. Complete Built-in Command Reference

CrossShellKSH includes over 45 built-in commands:

### Directory & Navigation
- **`cd [path]`**: Changes current working directory. Accepts Unix paths (`/c/src`), Windows paths (`C:\src`), and shortcuts (`~`, `-`, `..`).
- **`pwd`**: Prints current working directory.
- **`dirs [-c|-l|-v|-p|+N|-N]`**: Prints, indexes, or clears (`-c`) the directory stack.
- **`pushd [path]`**: Pushes the current directory onto the stack and navigates to target.
- **`popd`**: Pops the top directory off the stack and navigates to it.

### Variable & State Management
- **`typeset [-a|-A|-n|-i|-r|-x|-u|-l|-L N|-R N|-Z N|-T]`**: Declares variables, arrays, namerefs, integers, uppercase/lowercase transformations, fixed-width formatting, and custom types.
- **`set [-x|-e|-u|-o option|-- arg...]`**: Configures shell flags (`-x` trace, `-e` errexit, `-u` nounset, `-o vi`, `-o pipefail`) or assigns positional parameters (`$1`…`$N`).
- **`unset [-f] name...`**: Removes variables, array keys, or functions (`-f`).
- **`export name[=value]...`**: Marks variables for export to child processes.
- **`readonly name[=value]...`**: Marks variables as immutable.
- **`shift [N]`**: Shifts positional parameters left by $N$ places (default 1) and updates `$#`, `$*`, `$@`.
- **`getopts optstring name [args...]`**: Parses option flags and arguments in scripts.
- **`enum Name=(val1 val2...)`**: Creates an integer-backed enumeration type.

### Predefined Aliases
- **`integer`** → `typeset -i`
- **`float`** → `typeset -E`
- **`nameref`** → `typeset -n`
- **`functions`** → `typeset -f`
- **`autoload`** → `typeset -fu`
- **`compound`** → `typeset -C`
- **`history`** → `fc -l`
- **`r`** → `fc -s`
- **`type`** → `whence -v`

### Control Flow & Keywords
- **`if / then / elif / else / fi`**: Conditional execution branch.
- **`for var in list; do ... done`**: Word and argument iteration.
- **`while condition; do ... done`**: Loop while condition succeeds ($0$).
- **`until condition; do ... done`**: Loop until condition succeeds ($0$).
- **`case word in pattern) ... ;; esac`**: Multi-branch pattern matching.
- **`select var in list; do ... done`**: Formats interactive numbered menus using `$PS3` prompt and `$REPLY`.
- **`break [N]` / `continue [N]`**: Breaks or continues loop execution across $N$ levels.
- **`return [N]`**: Returns exit status from a shell function.
- **`exit [N]` / `logout [N]`**: Exits the shell session with status $N$.

### Condition Testing
- **`[[ expression ]]`**: Advanced ksh conditional evaluation with pattern matching (`==`, `!=`, `=~`).
- **`test expression` / `[ expression ]`**: Standard POSIX conditional evaluation.

### I/O & Diagnostics
- **`print [-n|-r|-p] [args...]`**: Writes text to stdout, raw text (`-r`), or co-process pipe (`-p`).
- **`echo [-n] [args...]`**: Compatible text printing.
- **`printf format [args...]`**: Formatted output supporting `%q` (shell escaping) and `%T` (timestamp formatting).
- **`read [-r|-s|-p prompt|-A name] [vars...]`**: Reads line from stdin, secret input (`-s`), or array fields (`-A`).
- **`clear`**: Clears terminal console buffer.

### History, Cache & Editing
- **`fc [-l|-n|-r|-e editor|-s [old=new]] [first] [last]`**: Lists history, re-executes commands, or opens history in `$FCEDIT` / `$VISUAL` / `$EDITOR` / `notepad`.
- **`hash [-r|-d name|-t name|-v name]`**: Manages internal executable path cache and hit tracking.

### Job & Process Control
- **`jobs [-l|-p] [job_id]`**: Lists active background jobs.
- **`fg [job_id]`**: Brings background job to foreground.
- **`bg [job_id]`**: Resumes background job.
- **`disown [-a|job_id...]`**: Removes jobs from shell job table without killing them.
- **`kill [-l|-s signal|-p|-j] pid|%job...`**: Sends termination or console signals to processes or jobs.
- **`wait [-p|job_id...]`**: Waits for background jobs or co-processes (`-p`) to finish.
- **`trap [-p|-] [action] [signals...]`**: Registers signal handlers (`EXIT`, `INT`, `BREAK`, `TERM`, `HUP`, `CHLD`, etc.).
- **`sleep seconds`**: Pauses execution with sub-second decimal precision.
- **`times`**: Reports user and system CPU time for shell and child processes.

### File & System Tools
- **`find [paths...] [-name|-iname|-type|-maxdepth|-mindepth|-empty|-size|-print|!]`**: Built-in file search.
- **`pathchk [-p|-P] pathname...`**: Validates path portability and length.
- **`getconf [var] [path]`**: Queries system parameters and configuration.
- **`umask [-S] [mask]`**: Displays or sets file creation mask.
- **`stty [options]`**: Queries or configures console modes (`echo`, `icanon`, `size`, `raw`, `cooked`).
- **`help [topic|command]`**: Displays built-in help and reference documentation.
- **`version`**: Displays CrossShellKSH version and host Windows release.

---

## 6. Interactive Line Editing (Vi Mode)

Interactive editing provides a modal Vi environment:

```ksh
# Enable Vi editing mode
set -o vi

# Disable Vi editing mode (standard mode)
set +o vi
```

### Vi Mode Keybindings

| Key / Motion | Mode | Action |
| :--- | :--- | :--- |
| `Esc` | Insert → Command | Enter Vi command mode |
| `i` / `a` / `A` / `I` | Command → Insert | Insert before cursor / append / append at end / insert at first non-blank |
| `h` / `l` | Command | Move cursor left / right |
| `0` / `$` | Command | Jump to beginning / end of line |
| `^` / `_` | Command | Jump to first non-blank character |
| `w` / `W` | Command | Jump forward by word / whitespace-delimited word |
| `b` / `B` | Command | Jump backward by word / whitespace-delimited word |
| `e` / `E` | Command | Jump to end of current word |
| `f<char>` / `F<char>` | Command | Find character forward / backward |
| `t<char>` / `T<char>` | Command | Move till character forward / backward |
| `;` / `,` | Command | Repeat last find motion forward / backward |
| `x` / `D` | Command | Delete character under cursor / delete to end of line |
| `k` / `j` | Command | Navigate up / down in command history |
| `[count]<motion>` | Command | Repeat any motion key (e.g. `3w`, `4h`, `2fX`, `5k`) |
| `Tab` | Insert | Trigger auto-completion for commands, files, and variables |

---

## 7. Prompt Customization & Startup Profile (`.kshrc`)

When started interactively, CrossShellKSH automatically loads `~/.kshrc` (located at `%USERPROFILE%\.kshrc`).

### Prompt Format Tokens (`PS1`)

| Token | Replacement Value |
| :--- | :--- |
| `%u` | Current username |
| `%d` | Active Windows domain or workgroup |
| `%w` | Current working directory (formatted with `~` for home) |
| `%m` | Machine / Host name |
| `%#` | `#` for elevated Administrator sessions, `$` for standard user |
| `%%` | Literal `%` character |

### Example `.kshrc` Configurations

#### Minimal Clean Prompt
```ksh
# ~/.kshrc
PS1='%w %# '
```

#### Developer Git & Powerline Prompt with ANSI Colors
```ksh
# ~/.kshrc
export EDITOR="notepad"
set -o vi

# ANSI Color Codes
C_BLUE="\033[38;2;97;175;239m"
C_GREEN="\033[38;2;152;195;121m"
C_PURPLE="\033[38;2;198;120;221m"
C_RESET="\033[0m"

# Custom prompt
PS1="${C_GREEN}%u@%m${C_RESET} ${C_BLUE}%w${C_RESET} ${C_PURPLE}%#${C_RESET} "

# Standard aliases
alias ll="ls -l"
alias gs="git status"
```

---

## 8. Windows Registry Integration

CrossShellKSH exposes the Windows Registry as dynamic shell variable namespaces:

- **`HKLM`**: `HKEY_LOCAL_MACHINE`
- **`HKCU`**: `HKEY_CURRENT_USER`

### Reading Registry Keys
```ksh
# Read Windows Product Name
win_name="${HKLM.SOFTWARE/Microsoft/Windows NT/CurrentVersion.ProductName}"
print "Operating System: $win_name"

# Read Current User Environment Variable
temp_dir="${HKCU.Environment.TEMP}"
print "User Temp: $temp_dir"
```

*Note: Use `/` as the path separator when Registry keys contain literal period (`.`) characters, such as `SOFTWARE/Microsoft/.NETFramework`.*

### Writing to the Registry
```ksh
# Assigning to a Registry variable updates the corresponding Registry value
HKCU.Environment.MY_CUSTOM_VAR="Enabled"
```

---

## 9. Scripting Walkthrough & Practical Examples

### Example 1: Automated Deployment Script
```ksh
#!/usr/bin/env ksh
set -e
set -o pipefail

TARGET_DIR="C:/Deploy/App"
BACKUP_DIR="C:/Deploy/Backups"

print "=== Starting Deployment ==="

if [ ! -d "$TARGET_DIR" ]; then
    print "Creating target directory: $TARGET_DIR"
    mkdir -p "$TARGET_DIR"
fi

# Find and package previous artifacts
print "Archiving old logs..."
find "$TARGET_DIR" -maxdepth 2 -iname "*.log" -type f

print "Deployment complete."
```

### Example 2: Interactive Menu with `select`
```ksh
#!/usr/bin/env ksh

PS3="Choose an environment to deploy [1-4]: "
select env in "Development" "Staging" "Production" "Cancel"; do
    case "$env" in
        "Development")
            print "Deploying to DEV..."
            break
            ;;
        "Staging")
            print "Deploying to STAGING..."
            break
            ;;
        "Production")
            print -n "Are you sure you want to deploy to PROD? (y/n): "
            read confirm
            if [ "$confirm" == "y" ]; then
                print "Deploying to PRODUCTION!"
            else
                print "Aborted."
            fi
            break
            ;;
        "Cancel")
            print "Exiting menu."
            break
            ;;
        *)
            print "Invalid option: $REPLY"
            ;;
    esac
done
```

---

### Running Self-Tests
Verify the build by executing the built-in regression suite:

```powershell
.\bin\ksh.exe --self-test
```
