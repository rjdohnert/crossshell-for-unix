# CrossForge IDE

**CrossForge** is a full-screen, menu-driven text editor and lightweight IDE that runs entirely in the Windows console. It combines a multi-file source editor with syntax highlighting, automatic compiler/debugger detection, and a sandboxed build/run/test engine — all without leaving the terminal.

> **Version:** 3.0.2
> **Author:** Copyright (C) 2026, Roberto J. Dohnert
> **Platform:** Windows (console / TUI)
> **Run it:** `crossforge`

CrossForge is **interactive only** — it takes no command-line arguments. Just run `crossforge` and you're in.

---

## Table of Contents

1. [Quick Start (5 minutes)](#1-quick-start-5-minutes)
2. [The Interface at a Glance](#2-the-interface-at-a-glance)
3. [Editing Basics](#3-editing-basics)
4. [Working with Multiple Files](#4-working-with-multiple-files)
5. [Find, Replace, and Navigation](#5-find-replace-and-navigation)
6. [Supported Languages](#6-supported-languages)
7. [Build, Run, Test](#7-build-run-test)
8. [Debugging](#8-debugging)
9. [The Sandbox (Safety)](#9-the-sandbox-safety)
10. [Themes](#10-themes)
11. [Configuration & Environment Variables](#11-configuration--environment-variables)
12. [Full Keyboard Reference](#12-full-keyboard-reference)
13. [Menu Reference](#13-menu-reference)
14. [Troubleshooting](#14-troubleshooting)

---

## 1. Quick Start (5 minutes)

Follow along — this is the fastest way to see CrossForge do something real.

1. **Start it:**
   ```
   crossforge
   ```

2. **Open a file:** Press `F3` (or `Ctrl+O`), type a path, press Enter.
   - Don't have one? Press `Ctrl+N` for a new buffer instead.

3. **Type something.** If it's a `.cpp`, `.cs`, `.js`, etc., you'll see syntax highlighting automatically.

4. **Save:** Press `F2` (or `Ctrl+S`).

5. **Build it:** Press `F9` (or `Ctrl+B`).
   - CrossForge detects the language from the file extension and runs the matching compiler.
   - Compiler output appears in the Compiler Output view (`Ctrl+Shift+O`).

6. **Run it:** Press `F10` (or `Ctrl+R`). Your program runs in a sandboxed process and its output appears on screen.

7. **Exit:** Press `Alt+X`.

That's the whole loop: **open → edit → build → run**.

---

## 2. The Interface at a Glance

CrossForge uses a classic Turbo Vision-style layout:

```
┌──────────────────────────────────────────────────────────────┐
│ File  Edit  Search  Run  Compile  Debug  Options  Window  Help│  ← Menu bar (top)
├──────────────────────────────────────────────────────────────┤
│ ┌─ main.cpp ──────────────────────────────┐                  │
│ │  1 | #include <iostream>                │  ← Editor window │
│ │  2 | int main() { ... }                 │    (one per file)│
│ └─────────────────────────────────────────┘                  │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ CrossForge | F1 Help | F2 Save | F3 Open | F4 Diagnostic ... │  ← Status bar (bottom)
└──────────────────────────────────────────────────────────────┘
```

- **Menu bar** (top): every feature lives here. Open it with the mouse or `Alt` + the highlighted letter.
- **Editor windows**: each open file is its own window with line numbers and highlighting.
- **Status bar** (bottom): always shows the most useful shortcuts so you don't have to memorize them.

---

## 3. Editing Basics

CrossForge behaves like a normal editor — what you type is what lands in the buffer.

| Action | Keys |
|---|---|
| New file | `Ctrl+N` |
| Open file | `Ctrl+O` or `F3` |
| Save | `Ctrl+S` or `F2` |
| Save As | `Ctrl+Shift+S` |
| Close window | `Ctrl+W` |
| Undo / Redo | `Ctrl+Z` / `Ctrl+Y` |
| Cut / Copy / Paste | `Ctrl+X` / `Ctrl+Shift+C` / `Ctrl+V` |
| Select all | `Ctrl+A` |
| Insert line | `Ctrl+Shift+I` |
| Delete line | `Ctrl+Shift+Bksp` |
| Duplicate line | `Ctrl+Shift+U` |

Tips:
- **Undo/Redo** is per-document, so you can safely experiment.
- A file with unsaved changes is marked modified in its title bar.
- If you close a modified file, CrossForge will prompt before discarding it.

---

## 4. Working with Multiple Files

Each file opens in its own editor window, and you can have many open at once.

| Action | Keys |
|---|---|
| New window | `Ctrl+Alt+N` |
| Switch to next window | `Ctrl+Tab` |
| Reopen most recent file | `Ctrl+E` |

The active window has a highlighted border; inactive windows are dimmed. Use `Ctrl+Tab` to cycle through them quickly.

---

## 5. Find, Replace, and Navigation

| Action | Keys |
|---|---|
| Find | `Ctrl+F` |
| Find next | `Ctrl+G` |
| Find previous | `Ctrl+Shift+G` |
| Replace | `Ctrl+H` |

Replace shows a confirmation as it goes, and the status bar reports how many occurrences were replaced (e.g., `Replaced 3 occurrence(s).`).

---

## 6. Supported Languages

CrossForge detects the language from the file extension and applies the right syntax highlighting and toolchain. Out of the box it knows:

| Language | Highlighting | Build | Debug |
|---|---|---|---|
| C++ | ✔ (incl. C++20 keywords) | ✔ | ✔ |
| C | ✔ | ✔ | ✔ |
| C# | ✔ | ✔ (`csc`) | ✔ |
| Java | ✔ | ✔ | ✔ |
| Objective-C | ✔ | ✔ | ✔ |
| Swift | ✔ | ✔ | ✔ |
| TypeScript | ✔ | ✔ (`tsc`) | ✔ (`node --inspect`) |
| JavaScript / Node.js | ✔ | ✔ (`node`) | ✔ (`node --inspect`) |
| Pascal | ✔ | ✔ | ✔ |
| Fortran | ✔ | ✔ | ✔ |
| COBOL | ✔ | ✔ | ✔ |
| Plain text | — | — | — |

**If a compiler isn't found automatically**, add its location manually:
`Options → Add Compiler Path` (`Ctrl+Alt+P`).

---

## 7. Build, Run, Test

These commands live in the **Compile** menu and are the heart of the IDE loop.

| Action | Keys | What it does |
|---|---|---|
| Build | `F9` / `Ctrl+B` | Compiles the current file with its language's compiler. |
| Run | `F10` / `Ctrl+R` | Builds (if needed) and runs the program, showing output. |
| Test | `Ctrl+T` | Runs the project's tests (e.g., `ctest --output-on-failure` for CMake projects, or the built binary with `--test`). |
| Build Tools | `Alt+F9` | Re-scan the system for installed compilers/toolchains. |
| Compiler Output | `Ctrl+Shift+O` | Re-open the last build's output. |
| Next Diagnostic | `F4` | Jump to the next compiler error/warning. |
| Run Linker Cmd | `Ctrl+L` | Run a custom linker command (Options menu). |

**Workflow tip:** after a build with errors, press `F4` repeatedly to step through each diagnostic, fix it, save with `F2`, and rebuild with `F9`.

---

## 8. Debugging

CrossForge detects installed debuggers and can launch the current file under one.

| Action | Keys |
|---|---|
| Debug current file | `Ctrl+Alt+D` |
| View debug output | `Ctrl+Shift+Y` |
| Debugger Tools (autodetect) | `Ctrl+Shift+D` |

The **Debugger Tools** dialog lists every debugger CrossForge found on your system (for example `vsdbg.exe` for C#/MSVC, or `node --inspect` for JavaScript/TypeScript). Pick one and CrossForge wires it into the Debug command.

---

## 9. The Sandbox (Safety)

When CrossForge runs your code (build, run, test, or debug), it does so inside a **restricted Windows Job Object** — a kernel-level sandbox. This means a misbehaving or runaway child process can't:

- consume unlimited CPU/time (it times out),
- spawn an unbounded number of child processes,
- escape and affect the rest of your session.

You'll see clear status messages for these cases, for example:
- `[CrossForge] Process timed out after 5000 ms.`
- `[CrossForge] Process cancelled by user.`
- `[CrossForge] Process exited with code 0.`

This makes CrossForge safe for experimenting with untrusted or buggy code.

---

## 10. Themes

Don't like the colors? Cycle through five built-in themes:

`Options → Next Theme` (`Ctrl+Alt+T`)

| Theme | Look |
|---|---|
| **Borland Blue** | Classic blue editor background (default). |
| **Monokai** | Dark, modern, soft-contrast palette. |
| **Matrix** | Green-on-black. |
| **Cyberpunk** | Neon accent colors. |
| **Retro Amber** | Amber-on-dark, old-terminal feel. |

The status bar confirms the change with `Theme changed.`

---

## 11. Configuration & Environment Variables

CrossForge stores per-user settings in a config directory:

```
%USERPROFILE%\.crossforge
```

It remembers recent files, the selected theme, and any compiler/debugger paths you've added.

Two environment variables tune the sandbox limits (useful if a legitimate build is long-running):

| Variable | Meaning | Example |
|---|---|---|
| `crossforge_PROCESS_TIMEOUT_MS` | Max milliseconds a child process may run before being cancelled. | `set crossforge_PROCESS_TIMEOUT_MS=30000` (30 s) |
| `crossforge_PROCESS_MAX_CHILDREN` | Max number of child processes a sandboxed job may spawn. | `set crossforge_PROCESS_MAX_CHILDREN=64` |

Set them before launching `crossforge` to take effect.

---

## 12. Full Keyboard Reference

**General / File**
| Key | Action |
|---|---|
| `F1` | Help topics |
| `F2` / `Ctrl+S` | Save |
| `F3` / `Ctrl+O` | Open |
| `Ctrl+N` | New file |
| `Ctrl+E` | Recent file |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+W` | Close window |
| `Alt+X` | Exit |

**Edit**
| Key | Action |
|---|---|
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+X` / `Ctrl+Shift+C` / `Ctrl+V` | Cut / Copy / Paste |
| `Ctrl+A` | Select all |
| `Ctrl+Shift+I` | Insert line |
| `Ctrl+Shift+Bksp` | Delete line |
| `Ctrl+Shift+U` | Duplicate line |

**Search**
| Key | Action |
|---|---|
| `Ctrl+F` | Find |
| `Ctrl+G` / `Ctrl+Shift+G` | Find next / previous |
| `Ctrl+H` | Replace |

**Build / Run / Test / Debug**
| Key | Action |
|---|---|
| `F9` / `Ctrl+B` | Build |
| `F10` / `Ctrl+R` | Run |
| `Ctrl+T` | Test |
| `Alt+F9` | Detect build tools |
| `F4` | Next diagnostic |
| `F8` | Terminal |
| `Ctrl+Shift+O` | Compiler output |
| `Ctrl+Alt+D` | Debug current file |
| `Ctrl+Shift+Y` | Debug output |
| `Ctrl+Shift+D` | Debugger tools |
| `Ctrl+L` | Run linker command |

**Window / Options / Help**
| Key | Action |
|---|---|
| `Ctrl+Tab` | Next window |
| `Ctrl+Alt+N` | New window |
| `Ctrl+Alt+T` | Next theme |
| `Ctrl+Alt+P` | Add compiler path |
| `Ctrl+Alt+A` | About |

---

## 13. Menu Reference

Every keyboard shortcut is also reachable from the menu bar. Here's the full map:

- **File** — New, Open, Recent File, Save, Save As, Close, Exit
- **Edit** — Cut, Copy, Paste, Undo, Redo, Select All, Insert Line, Delete Line, Duplicate Line
- **Search** — Find, Find Next, Find Previous, Replace
- **Run** — Terminal, Compiler Output, Next Diagnostic
- **Compile** — Build, Run, Test, Build Tools
- **Debug** — Debug Current, View Debug Output, Debugger Tools
- **Options** — Next Theme, Add Compiler Path, Run Linker Cmd
- **Window** — Next Window, New Window
- **Help** — Help Topics, About

---

## 14. Troubleshooting

**"No compiler found" when I press F9.**
The toolchain wasn't auto-detected. Add it manually via `Options → Add Compiler Path` (`Ctrl+Alt+P`), or re-scan with `Compile → Build Tools` (`Alt+F9`).

**My build times out.**
Long builds can hit the sandbox timeout. Raise it before starting CrossForge:
```
set crossforge_PROCESS_TIMEOUT_MS=60000
```

**A program spawned too many processes.**
That's the sandbox protecting you. If it's legitimate, raise the cap:
```
set crossforge_PROCESS_MAX_CHILDREN=128
```

**Colors look wrong / I want a different look.**
Press `Ctrl+Alt+T` to cycle themes until you find one you like.

**How do I get back to the command line temporarily?**
Press `F8` (Run → Terminal).

---

*CrossForge is part of the CrossShell for UNIX project. See `command-reference.md` for the full tool list and `docs/crossforge.md` for the condensed command reference.*
