# zsh

## What it does

- Starts an interactive zsh-like shell runtime on Windows
- Runs script files with zsh-style control flow and expansion support
- Executes one-shot commands via `-c`

## Options

- `zsh` - start interactive session
- `zsh script.zsh` - run a script file
- `zsh -c "command" [name [args...]]` - execute a command with zsh-compatible `$0` and positional arguments
- `-f` / `--no-rcs` - suppress startup files
- `-l` / `--login` - use login-shell startup and logout files
- `-n`, `-x`, `-e`, `-u`, `-v` - no-exec, xtrace, errexit, nounset, and verbose modes
- `--self-test` - run unit, portable-script conformance, pipeline, job-control, module, completion, widget, and console-event dispatch tests

## Built-in Commands

### Navigation

- `cd` / `chdir` - change directory; `-` returns to previous directory
- `pwd` - print current directory
- `pushd <dir>` / `popd` / `dirs` - directory stack navigation

### Output

- `echo`, `print`, `printf` - print text with various formatting options

### Variables and Environment

- `export`, `unset`, `typeset`, `integer`, `let` - variable and environment management
- `typeset` supports indexed/associative, exported, readonly, integer, unique, local, and global attributes (`-a`, `-A`, `-x`, `-r`, `-i`, `-U`, `-g`).
- Parameter expansion additions:
  - `${var:-word}` - use `word` when `var` is unset or empty
  - `${var:=word}` - assign and use `word` when `var` is unset or empty
  - `${var:+word}` - use `word` when `var` is set and non-empty
  - `${var:?message}` - report `message` when `var` is unset or empty
  - `${var#pattern}` - remove shortest matching prefix pattern
  - `${var##pattern}` - remove longest matching prefix pattern
  - `${var%pattern}` - remove shortest matching suffix pattern
  - `${var%%pattern}` - remove longest matching suffix pattern
  - `${(f)var}` - split newline-separated value into words
  - `${(q)var}` - shell-quote the value
  - `${(Q)var}` - remove one level of shell quoting
  - `${(j:sep:)array}` / `${(s:sep:)var}` - join or split using a delimiter
  - `${(P)var}` - indirect parameter lookup
  - `${(L)var}` / `${(U)var}` - convert the value to lower/upper case

### Input

- `read [options] [name ...]` assigns input fields to one or more variables; the final variable receives remaining fields.
- `read -A array` assigns all fields to an indexed array, `-r` preserves backslashes, and `name?prompt` displays a prompt.
- `read -d char` uses a custom delimiter, while `-N count` and `-k count` read an exact byte count.
- `read -t seconds` applies a timeout, `-s` disables console echo, and `-p` reads from the active coprocess.

### Windows Registry Namespaces

- `${HKLM.Key.Path.Value}` and `${HKCU.Key.Path.Value}` read Windows Registry values through dynamic namespaces.
- Use `/` as the Registry path separator when a key contains literal periods, for example `${HKLM.Software/Microsoft/.NETFramework.InstallRoot}`.
- Assignments such as `HKCU.Software.App.Setting=NewValue` update existing `REG_SZ` or `REG_EXPAND_SZ` values.
- Existing `REG_DWORD` and `REG_QWORD` values accept numeric assignments and retain their native type.
- Unsupported Registry types, including `REG_BINARY` and `REG_MULTI_SZ`, are rejected rather than silently converted.

### Flow Control

- `source` / `.`, `eval`, `return`, `exit`, `logout`, `true`, `false`, `test` / `[`, `break`, `continue` - script execution, conditions, and loop control
- `source [-e] [-x] [--] file [args...]` temporarily supplies positional arguments and can scope errexit or tracing to the sourced file.
- `logout` terminates login shells after running exit/logout cleanup; it reports an error in non-login shells.
- `break [n]` and `continue [n]` control loop execution across `for`, `while`, `until`, and `repeat` constructs.
- `[[ ... ]]` - extended tests with regex (`=~`), glob-aware string comparisons, unary checks, and logical operators

### Scripting Blocks

- `if ...; then ...; elif ...; else ...; fi`
- `while ...; do ...; done` / `while ... { ... }`
- `until ...; do ...; done` / `until ... { ... }`
- `repeat <count>; do ...; done` / `repeat <count> { ... }` / `repeat <count> <command>`
- `for name in a b c; do ...; done`
- `for (( init; cond; step )); do ...; done`
- `case word in pat) ... ;; pat2) ... ;& pat3) ... ;;& ... esac`
- Command chaining: `;`, `&&`, `||`

### History

- `history [-c]`, `fc [-l] [n]` - history listing and recall

### Job Control

- `jobs`, `fg`, `bg`, `disown`, `wait`, `kill` - background process management
- Native pipelines are grouped in Windows Job Objects; `kill %n` terminates the tracked pipeline and its assigned descendants.
- Jobs have stable numeric IDs and explicit running, stopped, done, or failed state. `%+`/`%%` selects the current job, `%-` the previous job, and `%?text` searches command text.
- `fg` and `bg` resume stopped Windows processes; `disown` releases shell ownership without terminating the process.
- Background pipeline creation validates Job Object assignment and thread resume. A partially launched pipeline is terminated and cleaned up rather than registered as a job.

### Utilities

- `dir` - lists directory contents via cmd.exe
- `times`, `getopts`, `trap`, `which`, `type`, `whence` - process, option parsing, and lookup
- `getopts` supports clustered short options, attached or separate option arguments, leading-colon diagnostics, and reset through `OPTIND=1`.
- `whence -w` reports command kinds, `whence -p` searches only `PATH`, and `whence -a` reports every matching alias, function, builtin, and executable.
- `type` and `which` use the same ordered alias, function, builtin, and executable lookup; `-a` reports all matches and `-p`/`-P` restrict lookup to executable paths.
- `times` reports shell CPU time followed by CPU time accumulated from reaped native children and jobs.
- `command` runs commands while bypassing alias/function wrappers; `-v` prints the resolved value, `-V` describes it, and `-p` uses the Windows system utility path.
- `builtin` invokes registered shell builtins directly, including `readonly`, while bypassing same-named functions.

### Completion and Modules

- `compinit` scans array `fpath` or semicolon-delimited `FPATH`, registers discovered `_command` functions, and initializes Tab completion.
- `compdef` routes argument completion through registered functions; `compadd` supplies candidates through `reply`.
- `zmodload zsh/datetime` exports `EPOCHSECONDS` and `EPOCHREALTIME` and enables `strftime`.
- `zmodload zsh/system` exports `SYS_PID` and enables descriptor-aware `sysread` and `syswrite`.
- `zmodload zsh/parameter` refreshes the `parameters`, `commands`, `functions`, and `aliases` associative arrays.
- `rehash` invalidates the shell's command-path lookup state; Windows process lookup remains delegated to `CreateProcess` and `PATH`.

## Redirection and Substitution

- Redirection support:
  - `>`, `>>`, `<`, `2>`, `2>>`
  - `<<<` (here-string)
  - `<<` and `<<-` here-doc support in script execution path
  - Descriptor duplication/closure such as `2>&1`, `1>&-`, `0<&1`
  - Persistent descriptors 3-9 through `exec`, including `exec 3>file`, `exec 4>&3`, `exec 5<file`, and `exec 3>&-`
- Substitution support:
  - Command substitution: `$(...)`, ``...``
  - Process substitution:
    - `<(command)` input producer
    - `>(command)` concurrent output sink backed by a Windows named pipe
  - `setopt MULTIOS` connects the producer to a streaming tee stage and writes each chunk to every `>` / `>>` target while the producer runs.
  - `coproc command` starts a bidirectional Windows-pipe coprocess. Use `print -p`, `coproc -i` to close its input, `read -p`, and `coproc -` to release it.

## Arrays

- Indexed arrays are 1-based
- Parenthesized assignments such as `items=(one "two values" three)` create indexed arrays without requiring `typeset -a`.
- `setopt KSH_ARRAYS` enables zero-based indexed-array lookup
- Negative indexes count backward from the final element (`$arr[-1]` is the last element)
- Slicing: `$arr[lo,hi]` (inclusive)
- `${#arr}` reports the number of elements and `${arr[@]}` expands all elements
- `${(k)map}` expands associative-array keys; the default associative-array expansion returns values
- Appending:
  - `arr+=(item)`
  - `arr+=(a b c)`

## Parsing and Function Scope

- Command words retain single-quoted, double-quoted, escaped, and unquoted parts through expansion.
- Filename generation applies only to wildcard characters originating in unquoted text.
- Command words use ordered tilde, arithmetic, command, process, and parameter expansion phases.
- `local`, `typeset`, and `declare` inside a function create dynamically scoped scalar or array bindings that are restored when the function returns.
- `setopt LOCAL_OPTIONS` and `setopt LOCAL_TRAPS` restore option and trap state when a function returns.
- `setopt SH_WORD_SPLIT` applies `IFS` splitting to unquoted parameter expansions.
- Quoted `"$@"` expands to one field per positional parameter, preserving empty and whitespace-containing arguments.
- Function positional parameters are restored after nested function calls.
- Statement nodes distinguish simple commands, functions, conditionals, loops, groups (`{ ... }`), and isolated subshells (`( ... )`). Subshell bodies execute in a child shell process, so variable, option, function, and working-directory changes cannot leak into the parent.
- Nested parameter names such as `${${name}}` are resolved recursively.

## Globbing and Pipeline Status

- Behavioral options include `NOMATCH`, `NULL_GLOB`, `GLOB_DOTS`, `EXTENDED_GLOB`, and `PIPE_FAIL`.
- Brace alternatives and numeric ranges are supported, including `file.{c,h}` and `item-{1..5}`.
- Extended patterns support character classes, grouped alternatives such as `@(foo|bar)`, exclusions such as `!(pattern)` and `^pattern`, and `#` / `##` repetition in filename generation, `[[ ]]`, and `case` matching.
- Qualifiers support regular files `(.)`, directories `(/)`, modification age (`m+N` / `m-N`), modification-time ordering (`om` / `Om`), and result ranges such as `[1,5]`.
- Builtins, aliases, and shell functions can participate in pipelines. They execute in child shell processes with a serialized copy of the caller's variables, arrays, attributes, aliases, functions, and options; native executables retain the direct process path.
- Mixed pipelines use real Win32 anonymous pipes and start all shell/native stages concurrently; no transfer files are used between stages.
- `$pipestatus` contains each pipeline stage status; `PIPE_FAIL` returns the rightmost non-zero stage status.

## Configuration

- `~/.zshenv` - sourced whenever startup files are enabled
- `~/.zprofile`, `~/.zlogin`, and `~/.zlogout` - sourced for login shells
- `~/.zshrc` - sourced only for interactive shells and created with a commented template on first interactive run
- `~/.zsh_history` - command history with `Month Date Year Time` timestamps and Unix seconds (capped at 10 000 entries)
- `PROMPT` - left-side prompt format string
- `RPROMPT` - right-side prompt format string; shown only when there is room (auto-hidden when input would overlap)
- Trap storage keys (internal): `__trap_<SIG>` such as `__trap_INT`, `__trap_WINCH`, `__trap_TSTP`

## Line Editor Shortcuts

- `Up` / `Down` - browse command history
- `Left` / `Right` - move cursor
- `Home` / `End` - jump to start or end of line
- `Backspace` - delete character before cursor
- `Delete` - delete character at cursor
- `Tab` - file and command completion
- `Ctrl+A` - jump to start of line
- `Ctrl+E` - jump to end of line
- `Ctrl+U` - delete from start of line to cursor
- `Ctrl+K` - delete from cursor to end of line
- `Ctrl+W` - erase previous word
- `Ctrl+L` - clear screen and repaint current line
- `Ctrl+C` - cancel current input line (SIGINT trap aware)
- `Ctrl+Z` - trigger TSTP emulation; suspends latest tracked background job when present
- `Ctrl+R` - reverse history search (press repeatedly to cycle older matches)

## Completion and ZLE

- `compdef function command` dispatches the registered completion function for that command.
- `compdef -d command` removes registered definitions. Completion functions can inspect `PREFIX` and call `compadd`; returned candidates are filtered by prefix, sorted, and deduplicated.
- `zstyle` stores and lists compatibility style definitions; candidate cycling is handled by the built-in line editor.
- `zstyle -L` emits replayable definitions; `zstyle -d context [style]` removes matching definitions.
- `bindkey -M keymap key widget` changes editor key dispatch for supported emacs/vi keymaps. `-L` lists replayable bindings, `-r` removes a binding, and `-A` copies a keymap.
- `zle -N widget [function]` registers a function-backed widget; `-A` aliases and `-D` deletes widgets. `BUFFER` and `CURSOR` changes are applied to the active edit buffer.

## Prompt Expansion

The `PROMPT` and `RPROMPT` variables support zsh-style tokens:

- `%n` - username
- `%m` - hostname
- `%~` - current directory
- `%1~` - trailing folder name only
- `%#` - `#` if elevated, `%` otherwise
- `%?` - last command exit status
- `%F{colour}` / `%f` - set / reset foreground colour
- `%B` / `%b` - start / reset bold text
- `%U` / `%u` - start / reset underline
- Colours: `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, and `br_` bright variants

## Verification Helpers

- `zsh --self-test` is the authoritative dependency-free regression entry point. Its embedded portable-script corpus records expected behavior shared with real zsh without requiring zsh or network access on the target server.
- The self-test covers main-thread `INT`, `TSTP`, and `WINCH` dispatch. Physical Ctrl+C, Ctrl+Z, and console resize delivery still require an interactive Windows console smoke test.

- `.vscode/verify_zsh_heredoc_redir.ps1` - verifies here-string/heredoc/fd redirection behavior
- `.vscode/verify_zsh_psub_flags.ps1` - verifies process substitution and `${(f)}` / `${(q)}`
- `.vscode/verify_zsh_signal_traps.ps1` - verifies trap registration and interactive trap guidance
- `.vscode/verify_zsh_compat_items_1_9.ps1` - verifies parsing, options, arrays, attributes, shell-process pipelines and subshells, qualifiers, process substitution, and Windows job emulation

## Known Windows Differences

- Job control tracks process and Job Object handles and supports stable numeric, current/previous, and `%?substring` selection, but Windows does not provide Unix process groups or terminal foreground ownership.
- `Ctrl+Z` uses a Windows suspend/emulation path for tracked jobs and may differ from native zsh on Unix TTYs.
- Trap handling for signals like `INT`, `WINCH`, and `TSTP` is implemented through Windows console/event hooks rather than Unix signal delivery.
- Process substitution uses shell-managed temporary paths and deferred sinks; Unix FIFO identity and `/dev/fd` semantics are not available.
- Windows ACLs, file attributes, path rules, and executable lookup replace Unix mode bits, ownership, and permission semantics where no direct equivalent exists.
- Persistent descriptors 3-9 are shell-managed Win32 handles used by compatible builtins; Windows child processes still expose only stdin, stdout, and stderr through `STARTUPINFO` rather than Unix-style inherited descriptor numbers.
- Coprocesses use one Windows pipe pair and do not expose authentic zsh descriptor-array semantics.
- Here-doc and redirection behavior is aligned for scripting compatibility, but edge cases can differ when Windows command wrappers are involved.

## Architecture

- The implementation intentionally remains one self-contained C++17 translation unit in `src/zsh/zsh.cpp` for direct compilation on air-gapped Windows servers.
- It has no third-party runtime or build dependencies. Internal sections and helpers provide maintainability without requiring generated sources or source-file splitting.

## Exit Status

- `0`: success
- `1`: general error
- `127`: command not found

## UNIX origin

Zsh (Z shell) was created by Paul Falstad in 1990. It extends the Korn shell with interactive features, extensive completion, and programmable prompt capabilities.
