# autotool

## Overview

autotool is a native multi-command build utility that combines:

- automake-style Makefile template generation
- autoconf-style configure parsing and probe execution
- libtool-style compile/link/install/clean/finish workflows
- parallel native project builds
- Linux-to-Windows port automation through MSYS2

It is designed to provide practical Autotools workflows on Windows, Linux, and AIX without requiring shell wrappers.

## Synopsis

```text
autotool <subcommand> [options]
autotool help [subcommand]
```

Subcommands:

- automake
- autoconf
- libtool
- build
- port
- help

## Main Help

```text
autotool --help
autotool -h
autotool help
autotool help <subcommand>
```

## Subcommand: automake

Generates Makefile.in from Makefile.am and supports common directive parsing.

### Automake Syntax

```text
autotool automake [flags] [Makefile.am]
```

### Automake Flags

- -a, --add-missing: create helper stubs (install-sh, missing, compile, depcomp)
- -c, --copy: copy missing helper files (compatibility flag)
- -f, --force-missing: overwrite existing helper scripts (compatibility flag)
- -v, --verbose: print additional processing details
- -h, --help: show automake help

### Notes

- Default input file is Makefile.am.
- If input exists, output is written as stem.in.

## Subcommand: autoconf

Parses configure.ac macros, optionally runs live compile probes, writes config headers, and expands template files.

### Autoconf Syntax

```text
autotool autoconf [flags] [configure.ac]
```

### Autoconf Flags

- -r, --run: execute configure checks immediately
- -C, --config-cache: compatibility/cache flag
- -d, --debug: keep temporary test sources/log details
- --prefix=DIR: set installation prefix
- --bindir=DIR: set binary directory value
- --libdir=DIR: set library directory value
- CC=COMPILER
- CXX=COMPILER
- CFLAGS=FLAGS
- CXXFLAGS=FLAGS
- LDFLAGS=FLAGS
- LIBS=FLAGS
- -h, --help: show autoconf help

### Autoconf Behavior

- Without --run, a configure.cmd launcher is generated.
- With --run, checks and substitutions are executed and config/status files are created.

## Subcommand: libtool

Implements native compile/link/install/clean/finish actions using libtool-like semantics.

### Build Syntax

```text
autotool libtool --mode=compile [switches] input...
autotool libtool --mode=link [switches] input...
autotool libtool --mode=install [switches] source... destination
autotool libtool --mode=clean [switches] path...
autotool libtool --mode=finish [switches] directory
```

### Primary switches

- --mode=compile|link|install|clean|finish
- --tag=CC|CXX
- -o FILE, --output=FILE
- -shared, --shared
- -static, --static
- -module, --module
- -n, --dry-run
- -v, --verbose
- --silent
- -h, --help

### Compatibility switches

- --compiler=CMD
- --cflags=FLAGS
- --ldflags=FLAGS
- --libs=FLAGS
- --install-dir=DIR
- -rpath DIR, --rpath=DIR
- -release NAME
- -version-info X:Y:Z
- -avoid-version
- -no-install
- -export-dynamic

### Mode rules

- compile requires source input and supports single-source -o semantics
- link can accept source and object inputs; source inputs may be compiled first
- install copies one or more artifacts to a directory or explicit destination
- clean removes listed files
- finish validates runtime/staging directories

## Subcommand: build

Parallel native build engine for current-directory C/C++ sources.

### Syntax

```text
autotool build [flags]
```

### Build Flags

- -j N, --jobs=N: worker count
- -c, --clean: remove output artifacts first
- -v, --verbose: print resolved compile/link commands
- -f, --file=SPEC: compatibility/custom spec flag
- -h, --help: show build help

### Build Behavior

- Detects compiler family (MSVC, GCC, Clang, XLC).
- Scans current directory for .c and .cpp files.
- Uses timestamp-based incremental compilation.

## Subcommand: port

Runs an MSYS2-backed build pipeline for Autotools projects and can bundle runtime DLL dependencies.

### Port Syntax

```text
autotool port [flags] [source_dir]
```

### Port Flags

- --msys2=PATH: MSYS2 installation root
- --subsystem=NAME: MINGW64, CLANG64, or UCRT64
- --no-autoreconf: skip autoreconf step
- --no-dll-bundle: skip post-build DLL bundling
- -v, --verbose: detailed command logging
- -h, --help: show port help

### Workflow summary

- enters MSYS2 login shell
- runs autoreconf (optional), configure, and make -j4
- scans built executables for imported DLLs and bundles missing runtime DLLs

## Exit behavior

- Returns success on completed subcommands.
- Returns failure for unknown subcommands, missing required mode/switch values, or command execution failures.

## Examples

```text
autotool automake -a -v Makefile.am
autotool autoconf -r --prefix="C:\Libs" CC=cl.exe CFLAGS="/O2"
autotool libtool --mode=compile --tag=CXX -o build\hello.obj hello.cpp
autotool libtool --mode=link -shared -o build\hello.dll build\hello.obj
autotool build -j8 -v
autotool port --msys2="C:\msys64" --subsystem=MINGW64 C:\src\project
```
