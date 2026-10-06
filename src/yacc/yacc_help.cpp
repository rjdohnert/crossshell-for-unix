#include "yacc_help.hpp"

void DisplayHelp() {
    std::cout << R"(yacc(1)                 CrossShell for UNIX Reference Manual                 yacc(1)

    NAME
        yacc - LALR(1) parser generator

    SYNOPSIS
        yacc [OPTIONS] GRAMMAR_FILE
        yacc [OPTIONS]

    DESCRIPTION
        yacc reads the grammar specification in GRAMMAR_FILE and generates an
        LR(1) / LALR(1) parser in C or C++. The generated parser translates an
        input stream of tokens according to the syntactic rules and semantic
        actions defined in the grammar.

    OPTIONS
        -d, --defines[=FILE]
            Generate an external C/C++ header file containing token declarations
            and semantic union definitions.

        -b, --file-prefix=PREFIX
            Specify the prefix to prepend to output file names.

        -o, --output=FILE
            Specify the output C/C++ parser file path.

        -p, --prefix=PREFIX
            Change the default 'yy' symbol prefix to PREFIX for parser
            variables and functions (e.g. xxparse, xxlex, yylval).

        -t, --debug
            Include parser runtime debugging code and execution traces.

        -v, --verbose
            Write an extensive grammar conflict and automaton state description
            file (.output).

        -y, --yacc
            Maintain strict POSIX Yacc compatibility; output files are named
            y.tab.c and y.tab.h by default.

        -l, --no-lines
            Suppress '#line' directives in generated C/C++ source code.

        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    GRAMMAR STRUCTURE
        A standard Yacc grammar file consists of three sections separated by '%%':

            %{
            /* C/C++ declarations and header includes */
            %}

            /* Token and precedence declarations */
            %token NUMBER IDENTIFIER
            %left '+' '-'
            %left '*' '/'

            %%

            /* Grammar rules and semantic actions */
            expr: expr '+' expr   { $$ = $1 + $3; }
                | NUMBER          { $$ = $1; }
                ;

            %%

            /* C/C++ user code and helper subroutines */

    EXAMPLES
        yacc grammar.y
            Generate standard LALR(1) parser 'grammar.tab.c'.

        yacc -d -y calc.y
            Generate parser and token header in POSIX mode ('y.tab.c', 'y.tab.h').

        yacc -d -v -p math_ -o math_parser.c math.y
            Generate parser with custom prefix 'math_' and state report 'math.output'.

        yacc -d -o parser.cpp grammar.y
            Generate C++ parser source file.

    CrossShell for UNIX                                                     yacc(1)
)";
}

void DisplayVersion() {
    std::cout << "yacc (CrossShell) 7.0.2\n"
              << "Copyright (C) 2026 Roberto J Dohnert. All rights reserved.\n";
}
