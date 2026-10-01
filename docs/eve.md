# eve

## What it does

Provides a Windows console implementation of the OpenVMS EVE editor. It edits
multiple text buffers, command-mode operations, mouse input, syntax
highlighting, selection, undo/redo, and the Windows clipboard.

## Usage

`eve [FILE]`

When `FILE` is omitted, EVE opens a new buffer named `NONAME.TXT`.

## Navigation and Editing

- Arrow keys move the cursor.
- `Home` and `End` move to the start and end of the current line.
- `Page Up` and `Page Down` scroll by a page.
- `Tab` inserts a hard tab and displays it at four-column tab stops.
- `Backspace` and `Delete` remove complete UTF-8 code points.
- `Ctrl+Z` or `Esc` opens or closes the `Command:` prompt.
- `Shift` plus cursor, Home/End, or paging keys extends the selection.
- `Ctrl+Tab` indents the selected lines; `Ctrl+Shift+Tab` outdents them.
- `Ctrl+Y` redoes the latest undone edit. `UNDO` and `REDO` are available from the command prompt.
- `Ctrl+F` opens a prefilled `FIND` command; `F3` repeats the latest search.
- The mouse wheel scrolls the text viewport. Click and drag selects text.

## Commands

Enter commands at the `Command:` prompt.

- `HELP`: open the EVE help screen. The mouse wheel scrolls help; any key or click returns to the buffer.
- `OPEN <filename>`, `GET <filename>`, `INCLUDE <filename>`: load a file into the active buffer.
- `WRITE [filename]`, `SAVE [filename]`: atomically write the buffer to a file. Saving to a new filename adopts that filename for subsequent saves and keeps the former version as `.bak`.
- `EXIT`, `EX`: save the current file and exit.
- `QUIT`, `Q`: exit only when no unsaved changes exist; use `QUIT /FORCE` to discard edits.
- `CLOSE [/FORCE]`: close the current buffer; `/FORCE` discards its unsaved edits.
- `TOP`, `BOTTOM`: move to the beginning or end of the buffer.
- `GOTO <line>`, `LINE <line>`: move to a line number.
- `FIND <text>`: search in the current direction.
- `FINDNEXT`, `NEXT`: repeat the latest search.
- `UNDO`, `REDO`: restore or reapply the most recent buffer edit.
- `SET INSERT`, `SET OVERSTRIKE`: choose insertion or overwrite mode.
- `SET FORWARD`, `SET REVERSE`: choose search direction.
- `SET CASE ON|OFF`, `SET WORD ON|OFF`: control case sensitivity and whole-word matching.
- `SET LINE NUMBERS ON|OFF`, `SET HIGHLIGHT ON|OFF`, `SET WRAP ON|OFF`: control editor display modes.
- `SET LANGUAGE <name>`: override detected syntax language for Text, C/C++, Python, JavaScript, PowerShell, Shell, or BASIC.
- `SELECT`: mark the selection anchor at the cursor.
- `COPY`: copy selected text, or the current line when no range is selected.
- `REMOVE`, `CUT`: remove selected text, or the current line when no range is selected, and copy it to the Windows clipboard.
- `PASTE`, `INSERT HERE`: insert text from the Windows clipboard at the cursor.

The command prompt retains its 50 most recent commands. Up/Down recalls them;
Tab completes command names and the first matching path argument.

## Advanced Editing

- `SET TAB WIDTH <1-16>` changes the displayed width of hard tabs.
- `SET RECTANGLE ON|OFF` switches selection between stream and column modes.
- `MARK <name>`, `GOTO MARK <name>`, and `SHOW MARKS` manage line bookmarks.
- `RELOAD [/FORCE]` reloads a file that changed on disk. EVE warns when it detects an external modification.
- `SET WRAP ON|OFF` keeps the viewport at the left edge while viewing long lines.

EVE stores display and search preferences in its user configuration file. On a
controlled Ctrl+C or console-close exit, modified buffers are copied to the
temporary `eve-recovery` folder for recovery.

## Syntax Highlighting

EVE detects language from the filename extension and highlights C/C++, Java,
Python, Swift, Kotlin, Fortran, COBOL, Pascal, JavaScript, BASIC, PowerShell,
batch files, and POSIX-style shell scripts. C-family block comments, Pascal
brace comments, backtick strings, and backslash-continued strings preserve
highlight state across lines.

## Windows Behavior

EVE preserves UTF-8 BOM and CRLF/LF line-ending style when loading and saving.
Saves first write a temporary sibling, then replace the destination only after a
successful write; an existing destination is retained as a `.bak` file. EVE enables console mouse input and virtual-terminal rendering while active. On
normal exit, Ctrl+C, or Ctrl+Break it clears the viewport and scrollback before
restoring the original console modes.

## UNIX origin

Inspired by the OpenVMS Extensible Versatile Editor (EVE); implemented as a
Windows-native console editor.