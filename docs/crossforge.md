# crossforge

## What it does

CrossForge is a full-screen, menu-driven text editor and lightweight IDE for the Windows console. It combines a multi-buffer source editor with syntax highlighting, compiler/toolchain autodetection, and a sandboxed build/run/debug execution engine.

It ships with language configurations for C++, C, Java, C#, Objective-C, Swift, TypeScript, JavaScript, Node.js, Pascal, Fortran, COBOL, Rust, and Go, and it auto-detects installed compilers and debuggers on the host.

## Usage

`crossforge`

CrossForge is interactive and takes no command-line arguments. It opens into its TUI; use the status bar shortcuts and menus to work.

## Common keys

- `F1` Help
- `F2` / `Ctrl+S` Save
- `F3` / `Ctrl+O` Open
- `F4` Diagnostic
- `F8` Terminal
- View -> WSL Terminal opens the default WSL distribution.
- View -> SSH Terminal opens an interactive SSH session after prompting for `user@host`.
- `F9` / `Ctrl+B` Build
- `F10` / `Ctrl+R` Run
- `Ctrl+T` Test
- `Ctrl+Alt+D` Debug current file
- `Ctrl+F` Find, `Ctrl+G` next match, `Ctrl+Shift+G` previous match, `Ctrl+H` Replace
- `Ctrl+Z` Undo, `Ctrl+Y` Redo
- `Ctrl+E` open most recent file
- `Ctrl+N` new, `Ctrl+W` close window, `Ctrl+Tab` next window
- `Alt+X` Exit

## Menus

- **File**: New, Open, Recent, Save, Save As, Close
- **Edit**: Cut, Copy, Paste, Undo, Redo, Select All, line insert/delete/duplicate
- **Search**: Find, Find Next, Find Previous, Replace
- **View**: Windows Terminal, WSL Terminal, SSH Terminal, compiler output, next diagnostic
- **Compile**: Build, Run, Test
- **Debug**: Debug Current, View Debug Output, Debugger Tools (autodetect)
- **Options**: Next Theme, Add Compiler Path, Run Linker Command
- **Window**: Next Window, New Window
- **Help**: About

## Configuration and sandboxing

- Per-user configuration is stored under `%USERPROFILE%\.crossforge`.
- Build/run/test/debug child processes execute inside a restricted Windows Job Object (sandbox). Limits are tunable via the `crossforge_PROCESS_TIMEOUT_MS` and `crossforge_PROCESS_MAX_CHILDREN` environment variables.
- Compiler and debugger paths are autodetected (MSVC/Build Tools under Program Files, `csc`, `tsc`, `node`, `rustc`, `go`, etc.) and can be extended through Options → Add Compiler Path.
- Rust files (`.rs`) build with `rustc`; Go files (`.go`) build with `go build`.
- Build and Link output is appended with local timestamps to `%USERPROFILE%\.crossforge_compiler_output`.

## Examples

- `crossforge` — open the IDE, then press `F3` to open a file and `F9` to build.
- `crossforge` with `crossforge_PROCESS_TIMEOUT_MS=5000` — cap sandboxed child processes at 5 seconds.

## UNIX origin

Not a UNIX utility; a Windows-native console IDE. Conceptually similar to terminal editors/IDEs such as Turbo Vision-style environments or `micro`.
