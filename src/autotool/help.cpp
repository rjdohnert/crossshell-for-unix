/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * CrossShell for UNIX
 */

#include "help.hpp"
#include <iostream>

using std::cout;

void print_main_help() {
    cout << R"(autotool(1)              CrossShell for UNIX Reference Manual               autotool(1)

    NAME
        autotool - native build and Autotools compatibility engine

    SYNOPSIS
        autotool COMMAND [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        'autotool' is a native, zero-dependency C++ engine combining Makefile
        generation, live compiler probing, libtool-style archive and shared-library
        orchestration, multi-threaded parallel compilation, and Linux-to-Windows
        software porting.

    COMMANDS
        automake
            Generate 'Makefile.in' templates from 'Makefile.am'.

        autoconf
            Parse 'configure.ac', execute compiler probes, and write 'config.h'.

        libtool
            Compile, link, install, and clean native libraries and executables.

        build
            Multi-threaded native compilation engine for Windows, Linux, and AIX.

        port
            Automate Linux Autotools software builds on Windows via MSYS2.

        help
            Display comprehensive manual for specific subcommands.

    OPTIONS
        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        autotool automake -a Makefile.am
            Generate template with missing helpers.

        autotool autoconf -r --prefix="C:\Program Files\MyApp" CC="cl.exe"
            Configure with live compiler probing.

        autotool build -j8 -v
            Run 8 parallel compilation threads with verbose output.

        autotool port --msys2="C:\msys64" --subsystem="MINGW64" C:\src\tarball
            Port Linux autotools package to Windows.

    CrossShell for UNIX                                                    autotool(1)
)";
}

void print_automake_help() {
    cout << R"(autotool-automake(1)     CrossShell for UNIX Reference Manual       autotool-automake(1)

    NAME
        autotool-automake - generate Makefile.in templates

    SYNOPSIS
        autotool automake [OPTIONS] Makefile.am

    DESCRIPTION
        Parses 'Makefile.am' specifications and produces portable 'Makefile.in' files
        configured for MSVC, MinGW, or POSIX make utilities.

    OPTIONS
        -a, --add-missing
            Automatically provision required helper scripts ('install-sh', 'missing',
            'compile', 'depcomp').

        -c, --copy
            Copy missing files instead of symlinking (Windows default).

        -f, --force-missing
            Overwrite existing standard bootstrap scripts.

        -v, --verbose
            List all files processed and directives parsed.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool automake -a -v Makefile.am
            Provision missing scripts and generate Makefile.in.

    CrossShell for UNIX                                             autotool-automake(1)
)";
}

void print_autoconf_help() {
    cout << R"(autotool-autoconf(1)     CrossShell for UNIX Reference Manual       autotool-autoconf(1)

    NAME
        autotool-autoconf - process configure.ac and compiler probes

    SYNOPSIS
        autotool autoconf [OPTIONS] configure.ac

    DESCRIPTION
        Parses M4/Autoconf macros in 'configure.ac', executes live compiler probes,
        writes 'config.h' headers, and performs @VAR@ template substitution.

    OPTIONS
        -r, --run
            Execute live compiler checks immediately on Windows.

        -C, --config-cache
            Enable probe caching via 'config.cache'.

        -d, --debug
            Retain temporary 'conftest.c' diagnostic files.

        --prefix=DIR
            Set installation prefix (default: 'C:\Program Files\Package').

        --bindir=DIR
            User executable directory [PREFIX/bin].

        --libdir=DIR
            Object library directory [PREFIX/lib].

        CC=COMPILER
            Specify C compiler (e.g. 'cl.exe' or 'gcc').

        CXX=COMPILER
            Specify C++ compiler (e.g. 'cl.exe' or 'g++').

        CFLAGS=FLAGS
            Pass additional C compiler flags.

        CXXFLAGS=FLAGS
            Pass additional C++ compiler flags.

        LDFLAGS=FLAGS
            Pass linker flags.

        LIBS=LIBRARIES
            Pass default libraries.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool autoconf -r --prefix="C:\Libs" CC="cl.exe" CFLAGS="/O2"
            Run live probes and generate headers.

    CrossShell for UNIX                                             autotool-autoconf(1)
)";
}

void print_build_help() {
    cout << R"(autotool-build(1)        CrossShell for UNIX Reference Manual         autotool-build(1)

    NAME
        autotool-build - run parallel incremental native builds

    SYNOPSIS
        autotool build [OPTIONS]

    DESCRIPTION
        High-performance, multi-threaded native build engine. Auto-detects toolchains
        (MSVC, GCC, Clang, XLC) and performs parallel incremental compilation.

    OPTIONS
        -j, --jobs=N
            Number of parallel compilation threads (default: CPU cores).

        -c, --clean
            Clean build artifacts before compiling.

        -v, --verbose
            Print full compilation and link command lines.

        -f, --file=SPEC
            Specify custom build specification file.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool build -j8 -v
            Run build with 8 parallel worker threads and verbose output.

    CrossShell for UNIX                                                   autotool-build(1)
)";
}

void print_port_help() {
    cout << R"(autotool-port(1)         CrossShell for UNIX Reference Manual          autotool-port(1)

    NAME
        autotool-port - port GNU Autotools projects through MSYS2

    SYNOPSIS
        autotool port [OPTIONS] PROJECT

    DESCRIPTION
        Automates porting Linux/GNU Autotools projects to Windows using MSYS2 / MinGW-w64.
        Executes build pipelines and dynamically inspects PE headers to bundle DLLs.

    OPTIONS
        --msys2=PATH
            Path to MSYS2 installation (default: 'C:\msys64').

        --subsystem=NAME
            MSYS2 subsystem: MINGW64, CLANG64, UCRT64 (default: 'MINGW64').

        --no-autoreconf
            Skip running 'autoreconf -fiv' before configuration.

        --no-dll-bundle
            Skip dynamic PE header DLL import resolution.

        -v, --verbose
            Print detailed MSYS2 bash commands and outputs.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool port --msys2="C:\msys64" --subsystem="MINGW64" C:\src\project
            Port project using MINGW64 subsystem.

    CrossShell for UNIX                                                    autotool-port(1)
)";
}

void print_libtool_help() {
    cout << R"(autotool-libtool(1)      CrossShell for UNIX Reference Manual       autotool-libtool(1)

    NAME
        autotool-libtool - compile, link, install, and clean libraries

    SYNOPSIS
        autotool libtool --mode=MODE [OPTIONS] INPUT...

    DESCRIPTION
        Provides a native libtool-style engine for compile, link, install, clean, and
        finish workflows. It focuses on practical archive/shared-library orchestration
        for MSVC, GCC, Clang, and XLC without requiring shell scripts.

    OPTIONS
        --mode=compile
            Compile a source file into a native object.

        --mode=link
            Link objects/sources into an executable, static archive, or shared library.

        --mode=install
            Copy artifacts into a destination directory or explicit path.

        --mode=clean
            Remove generated objects and libraries.

        --mode=finish
            Validate staged runtime output directory (compatibility mode).

        --tag=CC|CXX
            Select C or C++ driver semantics (default: CXX).

        -o FILE
            Output path for compile/link mode.

        -shared, --shared
            Build a shared library / DLL.

        -static, --static
            Build a static archive / import library set.

        -module, --module
            Alias shared-library style module build.

        -n, --dry-run
            Print the native command without executing it.

        -v, --verbose
            Print resolved command lines and compatibility notes.

        --compiler=CMD
            Override the detected compiler or driver.

        --cflags=FLAGS
            Extra compile flags used by --mode=compile and source promotion in link mode.

        --ldflags=FLAGS
            Extra linker flags for --mode=link.

        --libs=FLAGS
            Extra libraries for --mode=link.

        --install-dir=DIR
            Explicit install destination for --mode=install.

        -rpath DIR
            Accepted for libtool compatibility; stored as runtime/install intent.

        -release NAME
            Accepted for compatibility; currently metadata only.

        -version-info X:Y:Z
            Accepted for compatibility; currently metadata only.

        -avoid-version
            Accepted for compatibility.

        -no-install
            Accepted for compatibility.

        -export-dynamic
            Forwarded to linker flags when meaningful.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool libtool --mode=compile --tag=CXX -o build\hello.obj hello.cpp
            Compile source file into object.

        autotool libtool --mode=link -static -o build\libhello.lib build\hello.obj
            Link static library.

        autotool libtool --mode=link -shared -o build\hello.dll hello.cpp
            Link shared library DLL directly from source.

        autotool libtool --mode=install build\hello.dll stage\bin
            Install DLL to target directory.

        autotool libtool --mode=clean build\hello.obj build\libhello.lib
            Clean generated object and library files.

    CrossShell for UNIX                                             autotool-libtool(1)
)";
}
