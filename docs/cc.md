# cc

## What it does

Intelligent Clang/GCC compiler wrapper for Windows NT. Detects and delegates to an installed Clang or GCC backend even when it is not on the system `PATH`, then forwards all standard compiler flags through unchanged.

## Usage

- `cc [wrapper-options] [compiler-options] files...`

## Wrapper options

- `--cc-compiler=<target>`: select backend — `clang`, `gcc`, or `auto` (default `auto`: prefers Clang, falls back to GCC)
- `--cc-show-path`: print the detected compiler binary path and exit
- `--cc-use-path=<path>`: force use of a specific compiler binary
- `--cc-verbose`: print wrapper diagnostics and the executed command line
- `--cc-dry-run`: locate the compiler and print the command without executing it
- `--cc-help`, `-help`: display the help manual and exit
- `--cc-version`: show wrapper version and exit

All other options (`-Wall`, `-O2`, `-I`, `-L`, `-c`, `-o`, etc.) are forwarded directly to the underlying backend compiler.

## Compiler search locations

When the compiler is not on `PATH`, `cc` probes, in order:

- Visual Studio LLVM toolset (`%ProgramFiles%\Microsoft Visual Studio\...`)
- Standalone LLVM Windows install (`%ProgramFiles%\LLVM\bin`)
- MSYS2 subsystems (`C:\msys64\{clang64,ucrt64,mingw64}\bin`)
- WinLibs / standalone MinGW (`C:\winlibs\bin`, `C:\mingw64\bin`)
- Scoop shims (`%USERPROFILE%\scoop\apps\{llvm,gcc}\current\bin`)
- Chocolatey tools (`%ProgramData%\chocolatey\bin`)
- Legacy MinGW & TDM-GCC (`C:\MinGW\bin`, `C:\TDM-GCC-64\bin`)
- Windows Registry App Paths (HKLM & HKCU)

## Examples

```text
cc main.c -o main.exe
cc --cc-compiler=gcc -O2 -Wall app.c -o app.exe
cc --cc-show-path
cc --cc-dry-run -c module.c
```

## Build

```text
src\cc\build.bat             (auto-detects clang / cl / gcc, installs to bin\)
src\cc\build.ps1             (same; -Clean, -NoInstall, -Compiler clang|cl|gcc)
```

## UNIX origin

Named for the traditional Unix `cc` compiler driver; this implementation is a Windows-native backend-detection wrapper rather than a compiler itself.
