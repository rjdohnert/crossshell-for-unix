#include "options.hpp"

void OptionParser::PrintHelp() {
    std::cout << R"(printf(1)               CrossShell for UNIX Reference Manual                 printf(1)

    NAME
        printf - format and print data according to format specifications

    SYNOPSIS
        printf [-v VAR] FORMAT [ARGUMENT...]
        printf [OPTIONS]

    DESCRIPTION
        printf formats and writes ARGUMENT(s) under the control of FORMAT to
        standard output or an environment variable. If more arguments are
        provided than specifiers in FORMAT, the format string is cyclically
        reused until all arguments are consumed.

    FORMAT CONVERSIONS
        %s
            String argument.

        %b
            String with backslash escape sequence expansion. A '\c' escape
            halts further output.

        %q
            Shell-quoted string suitable for reuse as shell input.

        %d, %i
            Signed decimal integer.

        %u
            Unsigned decimal integer.

        %o
            Unsigned octal integer.

        %x, %X
            Unsigned hexadecimal integer (lowercase / uppercase).

        %f, %F
            Decimal floating-point number.

        %e, %E
            Scientific notation exponential floating-point number.

        %g, %G
            Compact floating-point representation (%f or %e/%E).

        %a, %A
            Hexadecimal floating-point representation (C99/POSIX).

        %c
            Single character (first char of argument or numeric code).

        %p
            Pointer address in hexadecimal format.

        %(DATEFMT)T
            Date/time formatted using strftime. ARGUMENT is an epoch timestamp
            (seconds since 1970-01-01), -1, or 'now' for the current time.

        %%
            Literal '%' character.

    FLAGS AND FIELD SPECIFIERS
        -
            Left-justify the output within the specified field width.

        +
            Always display a sign (+ or -) for signed numeric conversions.

        <space>
            Prefix positive signed numbers with a space.

        0
            Zero-pad numeric output on the left up to the field width.

        #
            Alternate form (e.g., 0x prefix for %x, decimal point for floats).

        '
            Use thousands grouping separator according to locale.

        *
            Dynamic width or precision read from the argument list.

    ESCAPE SEQUENCES
        \a      Alert (bell) [0x07]
        \b      Backspace [0x08]
        \e, \E  Escape character [0x1B]
        \f      Form feed [0x0C]
        \n      Newline / linefeed [0x0A]
        \r      Carriage return [0x0D]
        \t      Horizontal tab [0x09]
        \v      Vertical tab [0x0B]
        \\      Literal backslash
        \'      Single quote
        \"      Double quote
        \0NNN   Octal byte value (1 to 3 digits)
        \xHH    Hexadecimal byte value (1 to 2 hex digits)
        \c      Halt further output immediately

    OPTIONS
        -v VAR
            Assign formatted output to environment variable VAR instead of stdout.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

        --
            End of option processing. Subsequent arguments are treated as format
            or argument strings.

    EXAMPLES
        printf "Hello, %s!\n" "World"
            Print formatted string with newline.

        printf "%05d\n" 42
            Print zero-padded integer (00042).

        printf "%-10s | %8.2f\n" "Item" 19.95
            Align text and floating-point values in columns.

        printf "%(%Y-%m-%d %H:%M:%S)T\n" -1
            Print current local date and time.

        printf -v GREETING "Hello %s" "User"
            Assign formatted text into environment variable GREETING.

        printf "%b\n" "Line 1\nLine 2\cLine 3"
            Expand embedded backslash escapes and halt at \c.

    CrossShell for UNIX                                                     printf(1)
)";
}

void OptionParser::PrintVersion() {
    std::cout << "printf (CrossShell) 5.0.0\n";
}

bool OptionParser::Parse(int argc, char* argv[], PrintfOptions& opts) const {
    if (argc < 2) {
        return false;
    }

    bool seen_double_dash = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!seen_double_dash) {
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                opts.show_help = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                opts.show_version = true;
                return true;
            } else if (arg == "-v" && i + 1 < argc && opts.format.empty()) {
                opts.varName = argv[++i];
                continue;
            } else if (arg == "--") {
                seen_double_dash = true;
                continue;
            }
        }

        if (opts.format.empty()) {
            opts.format = arg;
        } else {
            opts.args.push_back(arg);
        }
    }

    return !opts.format.empty();
}
