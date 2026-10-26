# rgx

## What it does
Searches files, directories, or standard input for matching lines using regular
expressions or fixed strings. Directories are searched recursively by default.
RGX is a native Windows C implementation no UNIX environments are required.

## Usage
```text
rgx [OPTIONS] PATTERN [PATH ...]
rgx [OPTIONS] -e PATTERN ... [PATH ...]
rgx [OPTIONS] -f PATTERN_FILE [PATH ...]
rgx --files [OPTIONS] [PATH ...]
```

With no paths, redirected input is searched when available; otherwise RGX
searches the current directory. An explicit `-` path reads standard input.
Use `--` to end option parsing when a pattern or path begins with a hyphen.
Short options can be grouped, values can be attached (such as `-nFi` or `-m10`),
and long value options accept either a separate value or `--option=value`.

## Options
### Matching
- `-e PATTERN`, `--regexp PATTERN`: add a search pattern; repeatable.
- `-f FILE`, `--file FILE`: read one pattern per line; repeatable. `-` reads patterns from stdin. An empty pattern file supplies no patterns.
- `-F`, `--fixed-strings`: interpret patterns literally.
- Fixed strings use UTF-8-aware linear-time KMP; preprocessing is reused across files in a worker.
- `-i`, `--ignore-case`: case-insensitive matching.
- `-s`, `--case-sensitive`: case-sensitive matching.
- `-S`, `--smart-case`: ignore case unless a pattern contains an uppercase character. The last case-selection option wins.
- `-v`, `--invert-match`: select nonmatching lines.
- `-w`, `--word-regexp`: require word boundaries around matches.
- `-x`, `--line-regexp`: require a match covering the entire line.
- `-U`, `--multiline`: search across line boundaries with a bounded decoded-input buffer.
- `--multiline-dotall`: enable multiline searching and let `.` match newlines.

### Output
- `-n`, `--line-number`: show one-based line numbers; enabled automatically for console output.
- `-N`, `--no-line-number`: suppress line numbers.
- `-H`, `--with-filename`: always show filename prefixes.
- `-I`, `--no-filename`: suppress filename prefixes.
- `-o`, `--only-matching`: print matching portions rather than entire lines. Zero-length matches are not printed.
- `-c`, `--count`: count selected lines per file; files with no selected lines produce no count record.
- `--count-matches`: count individual matches, including zero-length matches. With inverted matching, count selected nonmatching lines instead.
- `-l`, `--files-with-matches`: list paths containing matches.
- `--files-without-match`: list paths containing no selected matches.
- `-q`, `--quiet`: suppress normal output and stop once a selected result is found.
- `--color never|auto|always`: control ANSI coloring; default is `auto`.
- `-a`, `--text`: search input containing NUL bytes as text. Without this option, searching stops at a line containing a NUL byte.
- `--crlf`: remove the trailing carriage return from CRLF lines before matching and printing.
- `--json`: emit JSON Lines `begin`, `match`, `context`, `end`, and `summary` events. Data uses `text` for valid UTF-8 or base64 `bytes` otherwise; line-mode match events contain all submatches.
- `-b`, `--byte-offset`: show offsets in decoded input. UTF-16 offsets refer to UTF-8-decoded data, not original UTF-16 byte positions.
- `--column`: show one-based byte columns.
- `-r TEXT`, `--replace TEXT`: replace every match on a line. `$0` inserts the whole match and `$$` inserts a dollar sign; capture references are rejected.
- `-0`, `--null`: NUL-delimit filenames and filename prefixes.
- `--heading`, `--no-heading`: group results under filenames or use ordinary prefixes. Explicit `-I` still suppresses filenames.
- `--stats`: report matches, selected lines, files, decoded bytes, and elapsed time. JSON includes its own summary; rendered-byte counts exclude end/summary metadata.

### Context and Limits
- `-A N`, `--after-context N`: print up to `N` lines after a match.
- `-B N`, `--before-context N`: print up to `N` lines before a match.
- `-C N`, `--context N`: set both before and after context.
- `-m N`, `--max-count N`: limit selected lines per file; `0` selects no matching lines.

### Traversal
- `--hidden`: include dot-prefixed and Windows-hidden files and directories.
- `--no-ignore`: disable local ignore-file filtering. Hidden filtering remains active unless `--hidden` is also supplied.
- `-g GLOB`, `--glob GLOB`: include matching paths; prefix a pattern with `!` to exclude. Repeatable.
- `--max-depth N`: restrict traversal depth; `0` prevents descending into a supplied directory. Values above 512 are rejected.
- `--files`: list eligible files without searching their contents or requiring a pattern.
- `--no-require-git`: apply Git ignores outside discovered repositories too.
- `-t NAME`, `--type NAME`: include a builtin/custom file type; repeatable.
- `-T NAME`, `--type-not NAME`: exclude a file type; repeatable.
- `--type-list`: list builtin/custom definitions.
- `--type-add NAME:GLOB`: add patterns for a type; repeatable.
- `--type-clear NAME`: suppress builtin patterns; `all` suppresses every builtin. Custom additions are retained.
- `-j N`, `--threads N`: select Windows workers, up to 16. Default `0` chooses up to four based on processor count. Stdin, quiet mode, file listing, and a single explicit file stay serial. Each worker owns its matcher and thread-local error/work state. Results are atomic per file; file order is not guaranteed.

Globs support Unicode, `?`, `*`, `**`, zero-directory `**/`, ranges, escapes,
and bounded `{a,b}` alternatives. Matching CLI globs override local ignore
decisions. `.rgignore` outranks `.ignore`, then Git rules; deeper rules win
within a source priority. Git ignores require a discovered repository unless
`--no-require-git` is used. Global excludes are read directly from
`GIT_CONFIG_GLOBAL` or the user's `.gitconfig`, with the XDG `git/ignore`
default as fallback. No Git subprocess is needed.

### Information
- `-h`, `--help`: display help.
- `-V`, `--version`: display version information.

## Examples
```powershell
rgx -n "TODO|FIXME" .\src
rgx -Fi "connection failed" .\logs
rgx -g '*.c' -g '!generated/**' "CreateProcess" .\src
rgx -n -C 2 "error" .\logs\service.log
rgx --count-matches "[0-9]+" .\data.txt
rgx -f .\patterns.txt .\src
rgx --files --hidden .\src
rgx -F -- "-token" .\input.txt
rgx --json -t c "CreateProcess" .\src
rgx -g '**/*.{c,h}' -j 4 --stats "TODO" .\src
rgx -Uo "first\nsecond" .\input.txt
rgx -r '<$0>' "[0-9]+" .\data.txt
rgx --type-add 'config:*.cfg' -t config "enabled" .\settings
```

```cmd
type input.txt | rgx -n "warning"
```

## Exit Status
- `0`: at least one selected result was found, or a help/version request succeeded.
- `1`: no selected result was found.
- `2`: invalid arguments, input/output failures, invalid or unsupported regex syntax, or a safety limit was encountered.

In quiet mode, an already-found selected result takes precedence over search
errors when choosing the search exit status. Diagnostic messages may still
appear on standard error.

## Regex and Input Support
The built-in, bounded NFA supports groups, alternation, character classes and
ranges, `.`, `*`, `+`, `?`, `{n,m}`, lazy repetitions, anchors, word boundaries,
common character and hexadecimal escapes, and inline `i`, `m`, and `s` flags,
including scoped forms such as `(?s:...)`. `\A` and `\z` are absolute anchors;
multiline `^` and `$` can refer to individual lines.
Multiple patterns are alternatives; `-o` can emit multiple matches per line.
Windows command-line arguments are converted to UTF-8. UTF-8 BOM and UTF-16
little- or big-endian BOM input are recognized; invalid UTF-16 is reported.
Unicode folding handles supplementary characters and special equivalences
such as final sigma, long s, and Kelvin sign. `\p{...}` and `\P{...}` support
`L`, `Lu`, `Ll`, `Nd`, `M`, `P`, `ASCII`, `Word`, `White_Space`, `Greek`,
`Latin`, `Cyrillic`, and `Han`, with selected long-name and `Script=...` aliases.
Windows classifications and supplementary/script-block ranges are not complete,
version-pinned Unicode tables. Unknown properties are rejected.

## Compatibility and Safety
Backreferences, look-around, capture extraction/replacement, arbitrary Unicode
properties, conditional Git configuration/includes, type-definition inheritance,
and inverted multiline searching remain unsupported. Multiline replacement
requires `-o`; JSON cannot be combined with count/file-list modes. JSON provides
structured events, not exact ripgrep parity for every edge case. Ignore matching
does not reproduce every Git precedence/escaping rule. Reparse points are
skipped to avoid loops. Parallel results are spooled in temporary files to
prevent interleaved file records.

Resource limits include 64 MiB per input line and retained context text,
16 MiB budgets for option strings, ignore rules/scopes, and queued path strings,
8192 nodes/states per regex, 32768 aggregate regex nodes/states, and bounded
matching work. Exceeding a limit reports an error rather than silently returning
a partial successful result. Regex patterns and pattern-file lines are limited
to 64 KiB; context values are limited to 1,048,576 lines. Combined literal
patterns are limited to 1 MiB. Multiline input is limited to 64 MiB and
1,048,576 mapped lines/matches. Worker output is limited to 64 MiB per file;
its pending queue holds 64 paths within a 16 MiB string budget.

## Regression Tests
Permanent tests cover expected output, Unicode/encoding, malformed-pattern
fuzzing with a fixed seed, serial/parallel agreement, JSON parsing, and a
repetitive-input timing gate:

```powershell
.\src\rgx\test.ps1
.\src\rgx\test.ps1 -Compiler cl -FuzzCases 200
.\src\rgx\test.ps1 -NoBuild -ReferenceExecutable C:\Tools\ripgrep\rg.exe
```

The optional reference executable is only a test oracle, not a runtime/build
dependency. Without it, comparisons are reported as skipped. Tests clean their
own temporary directory. Builds remain direct C17 with Windows APIs and the C
runtime using `clang` or `cl`.

## Building
From the repository root, using a configured Windows C toolchain:

```powershell
.\src\rgx\build.ps1
.\src\rgx\build.ps1 -Compiler cl
.\src\rgx\build.ps1 -NoInstall
```

The scripts compile all C17 modules. By default they install `rg.exe` into the
repository's `bin/` directory; `-NoInstall` leaves it in the source directory.
Batch and shell scripts are also provided. The shell script is for a Windows
shell environment, not a Linux port. The original monolithic source is retained
as `src/rgx/rg.c.orig` and is not part of the modular build.