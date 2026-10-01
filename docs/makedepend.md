# makedepend

## What it does

Scans C and C++ source files, fully evaluates preprocessor macros and conditional directives, and appends object-to-header dependency rules to a Makefile. Supports MSVC, GCC, and Clang compiler environments.

## Options

- `-I <dir>`: add directory to the header search path
- `-D <symbol>[=<val>]`: define a preprocessor symbol (default value is `1`)
- `-U <symbol>`: undefine a preprocessor symbol
- `-f <file>`: specify target Makefile (default: `Makefile`)
- `-o <suffix>`: set object file suffix (default: `.obj`)
- `-p <prefix>`: set object file prefix path
- `-s <string>`: specify the dependency delimiter string
- `-a`: append dependencies instead of replacing the existing section
- `-v`, `--verbose`: enable verbose output during dependency scanning
- `--compiler <name>`: select compiler preset — `msvc` (default), `gcc`, or `clang`; sets appropriate predefined macros, default object suffix, and enables include-path detection
- `--detect-cl`: auto-detect MSVC system include paths from `%INCLUDE%`
- `--msvc-ver <ver>`: set MSVC version simulation (default: `1930` for MSVC 2022)
- `--detect-gcc`: probe `g++` to discover GCC system include paths
- `--gcc-ver <ver>`: set GCC version for macro simulation (default: `13`)
- `--detect-clang`: probe `clang++` to discover Clang system include paths
- `--clang-ver <ver>`: set Clang version for macro simulation (default: `17`)
- `-h`, `--help`: show help text
- `--version`: show version information

## Usage

- `makedepend [options] [--] sourcefile ...`

## Examples

- `makedepend -I.\include -D_DEBUG -f Makefile main.cpp utils.cpp`
- `makedepend --detect-cl -I C:\MyProject\inc -o .obj -f Makefile.win src\*.cpp`
- `makedepend --compiler gcc --detect-gcc -o .o -f GNUmakefile src/*.cpp`
- `makedepend --compiler clang --detect-clang -I./include main.cpp`

## Exit Status

- `0`: success
- `1`: error

## UNIX origin

A standard X11/UNIX build tool for automatically generating header dependency rules in Makefiles.
