# CrossShellTCSH

**CrossShellTCSH** is a high-performance, standalone native Windows implementation of the **TENEX C Shell (tcsh / csh)** command language interpreter. Engineered from the ground up in C++ as a single-file zero-dependency engine, it delivers the complete C-shell interactive workflow, advanced word-list vector variables, path modifier syntax, arithmetic evaluation, and structured scripting directly on Microsoft Windows without requiring POSIX emulation layers like Cygwin, MSYS2, or WSL.

---

## Table of Contents

1. [Architectural Overview & Core Philosophy](#1-architectural-overview--core-philosophy)
2. [Key Strengths](#2-key-strengths)
3. [Differences from Standard Unix tcsh](#3-differences-from-standard-unix-tcsh)
4. [Shell Syntax & Language Features](#4-shell-syntax--language-features)
   - [Word-List Array Variables](#word-list-array-variables)
   - [Variable Path Modifiers](#variable-path-modifiers)
   - [Arithmetic Expressions (`@`)](#arithmetic-expressions-)
   - [History Expansions](#history-expansions)
   - [Piping & Redirection](#piping--redirection)
5. [Complete Built-in Command Reference](#5-complete-built-in-command-reference)
6. [Interactive Line Editing & Tab Completion](#6-interactive-line-editing--tab-completion)
7. [Prompt Formatting & Startup Profile (`.tcshrc`)](#7-prompt-formatting--startup-profile-tcshrc)
8. [Job Control & Win32 Process Management](#8-job-control--win32-process-management)
9. [Scripting Walkthrough & Practical Examples](#9-scripting-walkthrough--practical-examples)
10. [CLI Flags & Compilation](#10-cli-flags--compilation)

---

## 1. Architectural Overview & Core Philosophy

CrossShellTCSH is engineered specifically for Windows developers, system engineers, and DevOps administrators who require the expressiveness of the C-shell environment alongside the raw speed, portable single-binary deployment, and seamless filesystem interoperability of native Win32.

- **Zero External Dependencies**: Compiles directly against core Windows system libraries (`KERNEL32.dll`, `ADVAPI32.dll`, `SHELL32.dll`, `USER32.dll`) using standard C++. No Unix emulation runtimes or third-party DLLs are required.
- **Ultra-Fast Startup**: Boots in single-digit milliseconds (~4 ms) with instant terminal responsiveness.
- **Unified Path Semantics**: Automatically accepts both forward slashes (`/`) and backward slashes (`\`), absolute Windows drive roots (`C:\`), and tilde home shortcuts (`~`).
- **Hardened Execution Model**: Protected with RAII script depth guards (up to 64 nested levels), safe process handles, and bounded history buffers (1,000 entries).

---

## 2. Key Strengths

### 🚀 Native Win32 Performance & Portability
- Runs directly as a standalone portable executable (`bin\tcsh.exe`).
- Zero registry locking or installation rituals—drop it onto any Windows machine or USB drive and start typing.

### 📜 Classic C-Shell Syntax & Variable System
- **Word-List Vector Arrays**: Native support for list assignments `set names = ( alpha beta gamma )`, 1-indexed word lookups `$names[2]`, element counts `$#names`, and existence checks `$?names`.
- **C-Shell Path Modifiers**: Extract and manipulate paths instantly using `:h` (head), `:t` (tail), `:r` (root), `:e` (extension), `:u` (upper), and `:l` (lower).
- **History Selector Aliases**: Create powerful aliases using `\!*` (all arguments), `\!^` (first arg), `\!$` (last arg), and `\!:n` (nth arg).
- **C-Style Arithmetic (`@`)**: Evaluate mathematical expressions with operator precedence, parentheses, and modulo operations.

### 🖥️ Modern Console Integration & Tab Completion
- **UTF-8 Console Code Pages**: Configures `CP_UTF8` on both input and output for seamless Unicode and multilingual support.
- **VT100 / Virtual Terminal ANSI Processing**: Native 24-bit TrueColor support, ANSI escape sequence rendering, and cursor positioning via `echotc`.
- **Intelligent Autocompletion**: Dual-mode interactive Tab completion for shell built-ins, custom aliases, PATH executables, and directory trees.

### ⚙️ Win32 Job Object Process Supervision
- Tracks and isolates child processes using native Windows Job Objects.
- Full background task management: `&`, `jobs -l -p`, `fg`, `bg`, `stop`, `kill %job`, `disown`, and `wait`.

---

## 3. Differences from Standard Unix tcsh

While CrossShellTCSH faithfully reproduces the syntax and workflow of standard `tcsh`, several architectural distinctions exist due to native Windows internals:

| Feature / Domain | Standard tcsh (Unix / POSIX) | CrossShellTCSH (Windows Native) |
| :--- | :--- | :--- |
| **Process Spawning** | `fork()` and `execve()` | Win32 `CreateProcessW` with standard handle redirection pipes |
| **Path Formats** | Forward slashes only (`/usr/local/bin`) | Seamless forward (`/`) and backslashes (`\`), drive letters (`C:\`), and UNC paths |
| **Executable Lookup** | Executable permission bit (`chmod +x`) | Windows `SearchPathW` checking `.exe`, `.cmd`, `.bat`, `.com`, and `.ps1` |
| **Signal Handling** | POSIX signals (`SIGINT`, `SIGHUP`, `SIGTSTP`) | Win32 `ConsoleCtrlHandler`, `GenerateConsoleCtrlEvent`, and `onintr` |
| **Resource Limits** | Kernel `setrlimit` (`limit` / `unlimit`) | Managed shell variables for limits tracking |
| **File Permissions** | Octal POSIX modes (`0755`, `umask`) | Windows NTFS ACLs and Win32 attribute flags |
| **Terminal Mode** | `termios` / `ioctl` raw modes | Win32 Console Buffer API + `ENABLE_VIRTUAL_TERMINAL_PROCESSING` |

---

## 4. Shell Syntax & Language Features

### Word-List Array Variables
In TCSH, variables can be either scalar strings or lists of words enclosed in parentheses:

```csh
# Assign a word-list array
set servers = ( web01 web02 db01 cache01 )

# Access element by 1-based index
echo "Primary server is $servers[1]"

# Get array length ($#)
echo "Total servers: $#servers"

# Check if a variable is set ($?)
if ( $?servers ) echo "Servers variable is configured"

# Update an element in place
set servers[2] = "web02-backup"
```

### Variable Path Modifiers
Extract or modify filenames and paths directly without running external utilities:

```csh
set file = "C:/Projects/Source/main.cpp"

echo $file:h    # Head (directory) -> C:/Projects/Source
echo $file:t    # Tail (filename)  -> main.cpp
echo $file:r    # Root (no ext)    -> C:/Projects/Source/main
echo $file:e    # Extension        -> cpp
echo $file:u    # Uppercase        -> C:/PROJECTS/SOURCE/MAIN.CPP
echo $file:l    # Lowercase        -> c:/projects/source/main.cpp
```

### Arithmetic Expressions (`@`)
Perform C-style arithmetic evaluation:

```csh
# Addition, subtraction, multiplication, division, modulo
@ count = 10
@ total = ( $count * 5 ) + 2
@ remainder = 17 % 5

# Negative numbers and precedence
@ result = ( 50 - 100 ) / 2
echo "Result: $result"   # Outputs -25
```

### History Expansions
Quickly recall and reuse commands or arguments from history:

```csh
# Repeat the previous command
!!

# Substitute the last argument of the previous command
echo hello world
cat !$                  # Executes: cat world

# Substitute all arguments of the previous command
make target1 target2
ninja !*                # Executes: ninja target1 target2

# Execute the nth command from history
!42
```

### Piping & Redirection
Direct output, append data, or merge error streams:

```csh
# Standard output redirection
echo "Hello Windows" > output.txt

# Append to file
echo "New Line" >> output.txt

# Merge stdout and stderr redirection (TCSH >& syntax)
msbuild MyProject.sln >& build.log
ninja >>& build.log

# Standard pipelines and combined stderr pipeline (|&)
cat logs.txt | grep "ERROR" | wc -l
compile.cmd |& tee output.log
```

---

## 5. Complete Built-in Command Reference

CrossShellTCSH includes a comprehensive built-in command set:

### Variables & Environment
| Command | Description |
| :--- | :--- |
| `set` | Assign or list shell scalar and list variables (`set x = val`, `set list = ( a b )`). |
| `unset` | Remove shell variables (`unset var`). |
| `setenv` | Set an environment variable (`setenv NAME value`). |
| `unsetenv` | Remove an environment variable (`unsetenv NAME`). |
| `printenv` | Print shell variables and process environment variables. |
| `export` | Export variables into the process environment (`export VAR=val`). |
| `typeset` | Declare or display shell variables. |
| `read` | Read input from stdin into one or more shell variables. |
| `shift` | Shift positional arguments left (`$1` becomes `$2`, etc.). |

### Arithmetic & Logic
| Command | Description |
| :--- | :--- |
| `@` | Evaluate C-style arithmetic expressions (`@ x = $y * 2`). |
| `let` | Evaluate an expression and return status 0 if non-zero. |
| `test` | Evaluate conditional expressions (`==`, `!=`, `=~`, `!~`, `-e`, `-d`, `-f`, `-r`, `-w`, `-x`, `-z`). |
| `filetest` | Evaluate file test predicates on disk. |
| `eval` | Parse and execute arguments as a shell command line. |
| `:` | Null command; always succeeds (returns 0). |

### Navigation & Directory Stack
| Command | Description |
| :--- | :--- |
| `cd` / `chdir` | Change working directory. Supports `/`, `\`, `-` (previous directory), and `~`. |
| `pushd` | Push current directory onto the directory stack and switch to new path. |
| `popd` | Pop the top directory off the stack and switch to it. |
| `dirs` | Display the current directory stack. |
| `pwd` | Print current working directory. |

### Terminal & Display
| Command | Description |
| :--- | :--- |
| `echo` | Print arguments separated by spaces. |
| `print` | Print arguments to standard output. |
| `echotc` | Output ANSI terminal control sequences (`home`, `clear`, `bold`, `standout`, `cols`, etc.). |
| `clear` / `cls` | Clear the console window and reposition cursor at home position. |
| `settc` / `setty` | Set terminal capability attributes. |
| `telltc` | Display configured terminal capability parameters. |
| `termname` | Display or set the terminal type name. |

### Directory Listing & Lookups
| Command | Description |
| :--- | :--- |
| `ls-F` | List directory contents with indicator suffixes (`/` directory, `*` executable, `@` link). |
| `alias` | Define or list command aliases with argument selectors (`\!*`, `\!^`, `\!$`, `\!:n`). |
| `unalias` | Remove a defined alias. |
| `complete` | Define or inspect auto-completion rules. |
| `uncomplete`| Remove completion rules. |
| `which` / `where` | Locate binary executables on disk or report shell built-ins. |
| `whence` | Describe how a command name is resolved (alias, builtin, binary). |
| `hash` / `rehash` | Enable or refresh command lookup path caching. |
| `unhash` / `hashstat` | Disable lookup caching or display hash status. |

### Job Control & Signals
| Command | Description |
| :--- | :--- |
| `jobs` | List active background tasks (`-l` for PID, `-p` for PID-only). |
| `fg` | Bring a background job to the foreground. |
| `bg` | Resume a stopped background job. |
| `wait` | Wait for all or a specific background job to finish. |
| `kill` | Terminate a process by PID or job reference (`kill %1`). |
| `stop` / `suspend` | Suspend or stop a running job. |
| `disown` | Disassociate jobs from shell tracking. |
| `hup` | Terminate all active background jobs. |
| `notify` | Enable immediate notification when background jobs change state. |
| `onintr` | Control interrupt handling (`onintr -` ignore, `onintr label` trap to label, `onintr` restore). |
| `trap` | Install or inspect signal trap handlers. |

### Execution & Flow Control
| Command | Description |
| :--- | :--- |
| `repeat` | Repeat a command N times (`repeat 5 echo "Ping"`). |
| `time` | Measure and report wall-clock execution time of a command. |
| `exec` | Execute a command and exit the shell immediately. |
| `return` | Exit from a sourced script file with status code. |
| `select` | Render an interactive numerical selection menu. |
| `source` / `.` | Read and execute commands from a script file in the current shell. |
| `exit` / `logout` | Exit the shell session. |
| `history` | Display the command history buffer. |
| `log` | View or append entries to `~/.tcsh_log`. |
| `sched` | Schedule future command execution. |
| `alloc` | Display internal memory allocation statistics. |
| `ver` / `version` | Display shell version and dynamic Windows OS build details. |

---

## 6. Interactive Line Editing & Tab Completion

CrossShellTCSH provides a rich, interactive in-line editor with smooth arrow-key navigation and dual-context Tab completion:

```
% cd src/
% tc<Tab>          # Completes builtins or commands starting with 'tc' -> tcsh
% ls-F b<Tab>      # Path completion -> bin/
```

### Tab Completion Rules
1. **Command Completion (First Token)**:
   - Matches built-in commands (`echo`, `echotc`, `alias`, `pushd`, etc.).
   - Matches user-defined aliases.
   - Searches all directories on Windows `%PATH%` for executable files (`.exe`, `.bat`, `.cmd`, `.com`).
2. **Path Completion (Subsequent Tokens)**:
   - Matches directories and files relative to the current working directory or absolute paths.
   - Automatically appends a trailing `/` when matching directory targets.
3. **Candidate Cycling**:
   - First <kbd>Tab</kbd> expands to the longest common prefix.
   - If multiple candidates match, the shell prints candidate options and cycles through them on successive <kbd>Tab</kbd> keypresses.

### History Navigation
- <kbd>↑</kbd> (Up Arrow): Recall previous command from history.
- <kbd>↓</kbd> (Down Arrow): Recall next command from history.
- <kbd>Home</kbd> / <kbd>End</kbd>: Move cursor to start or end of the line.
- <kbd>←</kbd> / <kbd>→</kbd>: Move cursor left and right through the current buffer.

---

## 7. Prompt Formatting & Startup Profile (`.tcshrc`)

When launching in interactive mode, CrossShellTCSH automatically loads `~/.tcshrc` (or `~/.cshrc` in your user profile directory `C:\Users\<username>`). If none is found, a default `.tcshrc` is created automatically.

### Prompt Format Specifiers

Customize the `prompt` variable in `.tcshrc`:

| Specifier | Expands To | Example |
| :--- | :--- | :--- |
| `%n` | Current username | `rjdohnert` |
| `%d` | Current domain name | `CORP` |
| `%m` | Short computer hostname | `DESKTOP-WIN` |
| `%M` | Full computer hostname | `DESKTOP-WIN.domain.local` |
| `%~` | Current directory with `~` home prefix | `~/Projects/cmd-extended` |
| `%/` | Full absolute directory path | `C:\Source\cmd-extended` |
| `%c` | Basename of current directory | `cmd-extended` |
| `%#` | Role prompt character: `#` for Admin, `%` for User | `%` |
| `%?` | Return exit status of previous command | `0` |
| `%%` | Literal `%` character | `%` |

### Sample `~/.tcshrc` Configuration
```csh
# .tcshrc - CrossShellTCSH Configuration
# Set rich interactive prompt with user, host, path, and role indicator
set prompt = "[%n@%m %~]%# "

# Keep 1000 history entries
set history = 1000

# Convenient aliases
alias ll    "ls-F"
alias g     "git"
alias gs    "git status"
alias gd    "git diff"
alias build "ninja -C build"

# Synchronize PATH additions
set path = ( $path C:/tools/bin G:/scripts )
```

---

## 8. Job Control & Win32 Process Management

CrossShellTCSH manages processes through Windows Job Objects and Process Groups:

```csh
# Launch a background job
ping -t 127.0.0.1 > ping.log &

# List all tracked jobs with PIDs
jobs -l

# Output format:
# [1] 14280 Running    ping -t 127.0.0.1 > ping.log &

# Bring job 1 to the foreground
fg %1

# Send job back to background or terminate
kill %1

# Wait for all background tasks to finish
wait
```

---

## 9. Scripting Walkthrough & Practical Examples

CrossShellTCSH supports comprehensive CSH/TCSH scripting constructs.

### Example 1: `foreach` Loop & File Processing
```csh
#!/usr/bin/env tcsh

set src_files = ( src/arch.cpp src/awk.cpp src/cat.cpp src/tcsh.cpp )

echo "Processing source files..."
foreach file ( $src_files )
    echo "Inspecting $file:t (Head: $file:h, Ext: $file:e)"
    if ( -e $file ) then
        echo "  -> Found on disk"
    else
        echo "  -> Missing file!"
    endif
end
```

### Example 2: `switch / case` Construct
```csh
set build_type = "release"

switch ( $build_type )
    case "debug":
        echo "Building with debug symbols..."
        set cflags = "-g -O0"
        breaksw
    case "release":
        echo "Building optimized binary..."
        set cflags = "-O3"
        breaksw
    default:
        echo "Unknown build target!"
        breaksw
endsw
```

### Example 3: `while` Loop & Arithmetic
```csh
@ i = 1
@ max = 5

while ( $i <= $max )
    @ square = $i * $i
    echo "Value: $i -> Square: $square"
    @ i = $i + 1
end
```

### Example 4: Interrupt Trap (`onintr`) & Cleanup
```csh
# Arm interrupt trap to jump to 'cleanup' label on Ctrl+C
onintr cleanup

echo "Starting long task... (Press Ctrl+C to abort)"
sleep 30

cleanup:
echo "\nCleaning up temporary files..."
rm -f temp_*.tmp
echo "Done."
```

---

## 10. CLI Flags & Compilation

### Command-Line Arguments
```powershell
# Execute command string directly
tcsh.exe -c "set x = 10; @ y = $x * 4; echo y=$y"

# Run internal regression test suite (37+ tests)
tcsh.exe --self-test

# Start shell without reading ~/.tcshrc (fast mode)
tcsh.exe -f

# Display version and dynamic Windows release info
tcsh.exe --version

# Display help reference
tcsh.exe --help

# Execute a CSH script file with arguments
tcsh.exe build.csh target1 target2
```
---

## License

CrossShellTCSH is licensed under the **BSD-3-Clause License**.  
Copyright (C) 2026, Roberto J. Dohnert. All rights reserved.
