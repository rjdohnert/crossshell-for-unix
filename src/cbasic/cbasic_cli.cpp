#include "cbasic_common.hpp"
#include <iostream>

void printVersion() {
    std::cout << "CrossBASIC Interpreter & Virtual Machine v3.7\n";
    std::cout << "Copyright (C) 2026, Roberto J. Dohnert\n";
    std::cout << "Inspired by Microsoft BASIC by Bill Gates and Paul Allen\n\n";
}

void printHelp() {
    std::cout << R"HELP(cbasic(1)               CrossShell for UNIX Reference Manual                cbasic(1)

    NAME
        cbasic - Run Microsoft BASIC-compatible programs and statements.

    SYNOPSIS
        cbasic [FILE [--dump]]
        cbasic [--help | --version]

    DESCRIPTION
        Compiles BASIC source into bytecode and executes it in the CrossBASIC
        virtual machine. With no file, cbasic starts an interactive environment
        for entering, editing, saving, loading, and running BASIC programs.
        File access is restricted to the current working directory sandbox and
        diagnostics are written to the standard Windows console streams.

    OPTIONS
        --dump
            Print the bytecode disassembly, symbol table, and static data
            buffer before executing FILE.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and copyright information.

    REPL COMMANDS
        RUN
            Compile and execute the program in memory.

        LIST
            List the line-numbered BASIC program in memory.

        NEW
            Clear the program in memory.

        SAVE FILE
            Save the program in memory within the current directory sandbox.

        LOAD FILE
            Load a BASIC program from the current directory sandbox.

        HELP
            Display this reference manual.

        VERSION
            Display version and copyright information.

        EXIT
            Exit the interactive environment.

    EXAMPLES
        cbasic
            Start the interactive BASIC environment.

        cbasic game.bas
            Compile and execute game.bas.

        cbasic script.bas --dump
            Display compiled program details, then execute script.bas.

    CrossShell for UNIX                                                       cbasic(1)
)HELP";
}

