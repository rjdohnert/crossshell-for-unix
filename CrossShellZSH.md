# CrossShellZSH

**CrossShellZSH** is a high-performance, standalone native Windows implementation of the **Z Shell (zsh)** runtime. Written in modern C++ as a single-file, zero-dependency translation unit, CrossShellZSH brings the advanced interactive line editing, rich parameter expansions, programmable completions, mathematical evaluations, and sophisticated scripting capabilities of zsh directly to Microsoft Windows without requiring POSIX emulation environments like Cygwin or MSYS2.

---

## Table of Contents

1. [Architectural Overview & Core Philosophy](#1-architectural-overview--core-philosophy)
2. [Key Strengths](#2-key-strengths)
3. [Differences from Standard Unix ZSH](#3-differences-from-standard-unix-zsh)
4. [Advanced Language & Syntax Features](#4-advanced-language--syntax-features)
5. [Complete Built-in Command Reference](#5-complete-built-in-command-reference)
6. [Zsh Line Editor (ZLE) & Interactive Keybindings](#6-zsh-line-editor-zle--interactive-keybindings)
7. [Completion System (`compinit`, `compdef`, `zstyle`)](#7-completion-system-compinit-compdef-zstyle)
8. [Prompt Customization (`PROMPT` & `RPROMPT`)](#8-prompt-customization-prompt--rprompt)
9. [Windows Registry Integration](#9-windows-registry-integration)
10. [Scripting Walkthrough & Practical Examples](#10-scripting-walkthrough--practical-examples)
11. [Building & Verification](#11-building--verification)

---

## 1. Architectural Overview & Core Philosophy

CrossShellZSH is built specifically for Windows developers, system administrators, and automated pipelines that need the expressiveness of zsh with the performance and predictability of a native Win32 binary.

- **Zero External Dependencies**: Compiles directly against core Windows system DLLs (`KERNEL32.dll`, `ADVAPI32.dll`, `SHELL32.dll`, `USER32.dll`). No `msys-2.0.dll` or `cygwin1.dll` POSIX translation runtime is required.
- **Microsecond Startup Latency**: Starts instantly (~5 ms), avoiding the cold-boot delays associated with virtual machines, WSL, or heavy runtime DLLs.
- **Isolated Subshell Process Model**: Pipelines and parenthesized subshells `( ... )` execute in isolated child processes with serialized environment snapshots, guaranteeing that variable, option, and working-directory changes never leak into the parent shell.
- **Hardened Security & Depth Guards**: Protected against infinite recursion, stack overflows, and memory exhaustion with strict limits on expansion nesting (`64`), evaluation depth (`64`), and function recursion (`200`).

---

## 2. Key Strengths

### 🚀 Native Win32 Execution & Portability
- Standalone executable (`bin\zsh.exe`) that runs anywhere on Windows Server or client machines.
- Seamlessly handles Windows drive letters (`C:\`), forward slashes (`/`), backslashes (`\`), and UNC network shares (`\\server\share`).

### 🧩 True 1-Based ZSH Arrays & Slicing
- Standard 1-based array indexing matching real zsh (`$arr[1]` is the first element).
- Supports negative indexing (`$arr[-1]` is the last element), sub-range slicing (`$arr[2,4]`), and zero-based indexing via `setopt KSH_ARRAYS`.
- Literal array assignment syntax: `items=(one "two words" three)` and array appending `items+=(four five)`.

### 🔤 Comprehensive Parameter Expansion Flags
Supports zsh's signature parameter flag transformations:
- `${(f)var}`: Splits newline-separated strings into word arrays.
- `${(q)var}` / `${(Q)var}`: Shell-quotes or unquotes string contents.
- `${(j:sep:)array}` / `${(s:sep:)var}`: Joins array elements or splits strings on custom delimiters.
- `${(P)var}`: Indirect variable expansion.
- `${(L)var}` / `${(U)var}`: Case transformations (lowercase / uppercase).
- `${(k)map}`: Expands associative array keys.

### 🔍 Advanced Globbing & Qualifiers
- **Extended Glob Patterns**: Groups `@(a|b)`, exclusions `!(pat)` and `^pat`, and repetitions `#` and `##`.
- **Brace Expansions**: String alternatives (`file.{c,h}`) and numeric sequence ranges (`item-{1..10}`).
- **Glob Qualifiers**: Filter filenames by type (`*(.)` regular files, `*(/)` directories), modification age (`*(m-1)` modified in last day), and sort order (`*(om)` newest first).

### 🖥️ Modern Terminal & Dual Prompt Support (`PROMPT` + `RPROMPT`)
- **UTF-8 & Virtual Terminal Processing**: Native support for ANSI 24-bit true color, cursor controls, and Nerd Font / Powerline prompt glyphs.
- **Dynamic Right Prompt (`RPROMPT`)**: Displays on the right margin and automatically disappears if user input expands into its column space.

### 🧠 Intelligent Typo Correction
- Built-in Damerau-Levenshtein typo correction detects mistyped command names and suggests the closest matching builtin, alias, function, or executable.
- The `nocorrect` modifier disables correction for specific invocations.

---

## 3. Differences from Standard Unix ZSH

While CrossShellZSH implements the core syntax and behavior of zsh, certain platform adaptations are made for Windows:

| Feature / Domain | Standard Unix zsh | CrossShellZSH (Windows Native) |
| :--- | :--- | :--- |
| **Process Management** | `fork()` and `exec()` | Native Win32 `CreateProcessW` and Windows Job Objects |
| **Job Control** | POSIX job control with `SIGCONT` / `SIGSTOP` | Tracked Win32 process handles and Windows Job Objects |
| **Process Substitution** | `/dev/fd/N` and Unix FIFOs | Windows Named Pipes (`\\.\pipe\zsh_psub_*`) with background streaming relays |
| **File Descriptors** | POSIX VFS descriptors $0..N$ | Standard handles ($0, 1, 2$) mapped to Win32; descriptors $3..9$ supported via `exec` |
| **Signal Handling** | POSIX `SIGINT`, `SIGTSTP`, `SIGWINCH`, etc. | Windows Console event handlers (`CTRL_C_EVENT`, console resize, break traps) |
| **Windows Registry** | Not applicable | First-class variable namespace (`${HKLM...}` / `${HKCU...}`) |
| **Multios Streaming** | Unix kernel tee | Built-in streaming tee stage with concurrent pipe output |

---

## 4. Advanced Language & Syntax Features

### Parameter Expansion Flags
```zsh
# Split lines into an array
lines="apple
banana
cherry"
typeset -a items
items=( ${(f)lines} )
print "First item: $items[1]"   # apple

# Join array with delimiter
csv="${(j:,:)items}"
print "CSV: $csv"               # apple,banana,cherry

# Case conversions
title="cross shell"
print "${(U)title}"             # CROSS SHELL
```

### Extended Globbing & Qualifiers
```zsh
# List only directories
print -l *(/)

# List regular files sorted by modification time (newest first)
print -l *(om.)

# Match files excluding .log files
print -l ^*.log

# Numeric sequence expansion
for i in {1..5}; do
    print "Step $i"
done
```

### Loop Constructs & `repeat`
```zsh
# C-style for loop
for (( i = 1; i <= 5; i++ )); do
    print -n "$i "
done
print ""

# Convenient repeat loop
repeat 3 print "Hello from CrossShellZSH!"

# Pattern matching with fallthrough (;& and ;;&)
case "$opt" in
    debug) print "Debug enabled" ;&
    verbose) print "Verbose logging on" ;;
    *) print "Default mode" ;;
esac
```

### Multios Output Streaming
```zsh
# setopt MULTIOS is enabled by default
# Output is copied concurrently to both files and stdout
echo "Build started at $(date)" > build.log >> master.log
```

---

## 5. Complete Built-in Command Reference

### Directory & Navigation
- **`cd [path]` / `chdir [path]`**: Changes working directory. `cd -` returns to previous directory.
- **`pwd`**: Prints current directory.
- **`dirs`**: Displays directory stack.
- **`pushd [path]` / `popd`**: Pushes onto or pops from directory stack.

### Variable & Environment Management
- **`typeset` / `declare` / `local`**: Declares variables with attributes:
  - `-a`: Indexed array
  - `-A`: Associative array
  - `-i`: Integer
  - `-r`: Readonly
  - `-x`: Exported to child processes
  - `-U`: Keep unique values only (deduplicate)
  - `-g`: Global scope within functions
- **`integer name[=val]`**: Shorthand for `typeset -i`.
- **`export name[=val]...`**: Exports variables to child processes.
- **`unset [-f] name...`**: Unsets variables or functions (`-f`).
- **`let "expr"`**: Evaluates arithmetic expressions.

### Input, Output & Formatting
- **`print [options] [args...]`**: Outputs text; `-l` prints each argument on a new line, `-n` suppresses newline, `-r` prints raw, `-p` sends to coprocess.
- **`echo [args...]`**: Compatible text printing.
- **`printf format [args...]`**: Formatted printing with `%q` shell quoting support.
- **`read [options] [name ...]`**: Reads input; `-r` raw, `-s` silent/password, `-A array` array fields, `-p` coprocess, `name?prompt` inline prompt.
- **`clear`**: Clears terminal screen.

### Control Flow & Evaluation
- **`source file [args...]` / `. file`**: Sourced execution with optional positional arguments.
- **`eval "cmd"`**: Dynamic string evaluation.
- **`return [N]`**: Returns from function with exit status $N$.
- **`exit [N]` / `logout [N]`**: Exits shell session ($N$).
- **`true` / `false` / `:`**: Standard condition primitives.
- **`[[ expr ]]`**: Extended conditional test with regex (`=~`) and glob matching.
- **`test expr` / `[ expr ]`**: POSIX-style condition testing.

### Job & Process Management
- **`jobs [-l|-p]`**: Lists background jobs with stable numeric IDs.
- **`fg [%job]` / `bg [%job]`**: Brings to foreground or resumes background jobs.
- **`disown [%job]`**: Disowns jobs from shell management.
- **`wait [%job]`**: Waits for background jobs or coprocesses to finish.
- **`kill [-s signal] pid|%job`**: Sends signals to process IDs or job groups.
- **`trap [-p] [action] [signals...]`**: Manages signal handlers (`INT`, `WINCH`, `TSTP`, `EXIT`, `TERM`, `HUP`).

### ZLE, Completion & Modules
- **`compinit`**: Scans `$fpath` and initializes the programmable completion system.
- **`compdef function command`**: Assigns completion functions to commands.
- **`compadd [options] candidates...`**: Adds completion candidates inside completion widgets.
- **`zstyle context style value...`**: Configures completion styling and behaviors.
- **`bindkey [options] key widget`**: Maps keyboard shortcuts to ZLE widgets.
- **`zle -N widget [function]`**: Registers custom user-defined editing widgets.
- **`zmodload module`**: Loads compatibility modules:
  - `zsh/datetime`: Exports `EPOCHSECONDS`, `EPOCHREALTIME`, and `strftime`.
  - `zsh/system`: Exports `SYS_PID` and descriptor-aware `sysread`/`syswrite`.
  - `zsh/parameter`: Exposes `parameters`, `commands`, `functions`, and `aliases` associative arrays.

### Utilities & Helpers
- **`which` / `type` / `whence [-v|-a|-p|-w]`**: Reports command classification and resolution path.
- **`command [-v|-V|-p] cmd`**: Bypasses functions/aliases to run builtins or external binaries directly.
- **`builtin cmd`**: Executes shell builtins directly.
- **`rehash`**: Invalidates and flushes the executable path cache.
- **`times`**: Prints accumulated CPU execution time for shell and children.
- **`getopts optstring name [args...]`**: Parses option flags.
- **`nocorrect cmd`**: Runs command without spelling/typo correction.
- **`dir`**: Lists Windows directory contents via `cmd.exe`.

---

## 6. Zsh Line Editor (ZLE) & Interactive Keybindings

The interactive line editor provides Emacs and Vi keymaps with multi-line editing and history searching.

### Default Interactive Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl+A` / `Home` | Move cursor to start of line |
| `Ctrl+E` / `End` | Move cursor to end of line |
| `Ctrl+U` | Delete from cursor to start of line |
| `Ctrl+K` | Delete from cursor to end of line |
| `Ctrl+W` | Erase previous word |
| `Ctrl+L` | Clear screen and redraw current line |
| `Ctrl+R` | Incremental reverse history search (press repeatedly to cycle) |
| `Ctrl+C` | Cancel current command line |
| `Ctrl+Z` | Suspend active foreground job |
| `Tab` | Context-sensitive auto-completion |
| `Up` / `Down` | Browse previous / next command in history |

### Creating Custom ZLE Widgets
```zsh
# Define a custom widget that wraps the current line in quotes
quote_line_widget() {
    BUFFER="\"$BUFFER\""
    CURSOR=$#BUFFER
}

# Register and bind to Ctrl+Q
zle -N quote_line_widget
bindkey "^Q" quote_line_widget
```

---

## 7. Completion System (`compinit`, `compdef`, `zstyle`)

CrossShellZSH includes a fully functional programmable completion architecture:

```zsh
# Initialize completions
compinit

# Define custom completion function for a tool
_mytool_completions() {
    local -a subcommands
    subcommands=(build deploy test clean status)
    compadd -a subcommands
}

# Bind completion function to command
compdef _mytool_completions mytool
```

---

## 8. Prompt Customization (`PROMPT` & `RPROMPT`)

Prompts are configured via `PROMPT` (or `PS1`) and `RPROMPT`.

### Prompt Format Tokens

| Token | Replacement Value |
| :--- | :--- |
| `%n` | Current username |
| `%m` | Current machine hostname |
| `%~` | Current directory (formatted with `~` for user profile) |
| `%1~` | Trailing folder name only |
| `%#` | `#` for elevated Administrator session, `%` for normal user |
| `%?` | Exit code of previous command |
| `%F{color}` / `%f` | Set foreground color / reset color |
| `%B` / `%b` | Start bold text / reset bold |
| `%U` / `%u` | Start underline / reset underline |

*Supported Colors: `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, and bright variants (`br_green`, etc.).*

### Example Configurations

#### Modern ANSI Powerline Prompt with Right-Side Status
```zsh
# ~/.zshrc
export PROMPT="%F{green}%n@%m%f %F{blue}%~%f %F{magenta}%#%f "
export RPROMPT="%(?.%F{green}✔.%F{red}✘ %?%f)"
```

---

## 9. Windows Registry Integration

The Windows Registry can be read and modified directly through dynamic variable namespaces:

```zsh
# Read Registry Values
os_name="${HKLM.SOFTWARE/Microsoft/Windows NT/CurrentVersion.ProductName}"
build_num="${HKLM.SOFTWARE/Microsoft/Windows NT/CurrentVersion.CurrentBuild}"
print "Host OS: $os_name (Build $build_num)"

# Write Registry Value
HKCU.Environment.CUSTOM_FLAG="Active"
```

---

## 10. Scripting Walkthrough & Practical Examples

### Example 1: Batch Log Processor with Array Expansion
```zsh
#!/usr/bin/env zsh
set -e

typeset -a log_files
log_files=( C:/Logs/*.log(om) )

print "Found ${#log_files} log files:"
for file in $log_files; do
    print "Processing: $file (Size: $(stat -c %s $file 2>/dev/null || echo 'N/A'))"
done
```

### Example 2: Interactive Coprocess Communication
```zsh
#!/usr/bin/env zsh

# Launch coprocess
coproc python -c '
import sys
while True:
    line = sys.stdin.readline()
    if not line: break
    sys.stdout.write(f"ACK: {line.strip()}\n")
    sys.stdout.flush()
'

# Send and receive messages
print -p "Hello from CrossShellZSH"
read -p response
print "Coprocess replied: $response"

# Clean up coprocess
coproc -
```

---

### Running the Authoritative Regression Suite
CrossShellZSH includes an embedded 187-test regression suite:

```powershell
.\bin\zsh.exe --self-test
```
