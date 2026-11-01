# fsctl

## Overview
`fsctl` (File System Control) is a high-performance, full-screen interactive Terminal User Interface (TUI) file manager for Windows, inspired by Norton Commander, Midnight Commander, and `ranger`.

It provides rapid navigation across local and network storage volumes with vim-style controls, interactive filtering, clipboard copy/cut/paste, sorting, safe Recycle Bin deletion, file property inspection, and seamless file launching.

---

## Command Line Qualifiers
```
fsctl [QUALIFIERS]
```

- `-p, --path <DIR>`: Specify the initial working directory (default: `.`)
- `-a, --all`: Display hidden and system files
- `-s, --sort <MODE>`: Set sort mode (`name`, `size`, or `date`) [Default: `name`]
- `-r, --reverse`: Reverse the listing order
- `-R, --readonly`: Enable read-only mode (prevents file deletions)
- `-h, --help`: Display usage summary and keybindings
- `-v, --version`: Display version information

---

## Interactive Keybindings

### Navigation
- `Up` / `k`: Move cursor up
- `Down` / `j`: Move cursor down
- `Enter` / `l`: Open selected directory or launch file in default Windows application
- `Backspace` / `h`: Navigate up to parent directory
- `Home` / `End`: Jump to first / last item in directory

### Filter & Clipboard (v6.5.0+)
- `/`: Interactive substring search and live filter (Press `Esc` to clear)
- `y`: Copy (Yank) selected file or folder to clipboard buffer
- `x`: Cut selected file or folder to clipboard buffer
- `p`: Paste copied/cut file or folder into current directory

### Display & Sorting
- `a`: Toggle hidden and system files
- `s`: Cycle sort field (`Name`, `Size`, `Date`)
- `r`: Invert sort order direction (Ascending / Descending)

### Actions & Management
- `d`: Delete selected file or folder (Safely prompted; sent to Windows Recycle Bin)
- `R`: Rename selected file or directory
- `i` (or `p`): Show detailed file properties dialog (Size, timestamps, Win32 attributes)
- `v`: Drive selection menu (Volume selector with free/total storage)
- `n`: List mapped network drives (`WNetGetConnectionW`)
- `S`: Launch SSH terminal window in current directory
- `?` or `F1`: Open help overlay modal
- `q` or `Esc`: Exit fsctl and restore terminal buffer
