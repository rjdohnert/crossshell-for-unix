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
 */

#include <iostream>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <windows.h>

// ============================================================================
// 1. ESCAPE SEQUENCE & ARGUMENT PARSING
// ============================================================================

class EscapeSequenceParser {
public:
    static char ParseEscapeSequence(const std::string& str, size_t& i) {
        if (i >= str.length()) return '\\';
        char c = str[i++];
        switch (c) {
            case 'a': return '\a';
            case 'b': return '\b';
            case 'e': case 'E': return '\x1B';
            case 'f': return '\f';
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case 'v': return '\v';
            case '\\': return '\\';
            case '\'': return '\'';
            case '"': return '"';
            case '?': return '?';
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
                int val = c - '0';
                int count = 0;
                while (i < str.length() && count < 2 && str[i] >= '0' && str[i] <= '7') {
                    val = val * 8 + (str[i++] - '0');
                    count++;
                }
                return static_cast<char>(val);
            }
            case 'x': {
                int val = 0;
                int count = 0;
                while (i < str.length() && count < 2 && std::isxdigit(static_cast<unsigned char>(str[i]))) {
                    char h = str[i++];
                    int digit = 0;
                    if (h >= '0' && h <= '9') digit = h - '0';
                    else if (h >= 'a' && h <= 'f') digit = h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') digit = h - 'A' + 10;
                    val = val * 16 + digit;
                    count++;
                }
                return static_cast<char>(val);
            }
            default:
                return c;
        }
    }
};

class TypeCoercer {
public:
    static long long ParseIntArg(const std::string& str) {
        if (str.empty()) return 0;
        if (str[0] == '\'' || str[0] == '"') {
            if (str.length() > 1) return static_cast<unsigned char>(str[1]);
            return 0;
        }
        try {
            size_t idx = 0;
            return std::stoll(str, &idx, 0);
        } catch (...) {
            return 0;
        }
    }

    static double ParseDoubleArg(const std::string& str) {
        if (str.empty()) return 0.0;
        if (str[0] == '\'' || str[0] == '"') {
            if (str.length() > 1) return static_cast<double>(static_cast<unsigned char>(str[1]));
            return 0.0;
        }
        try {
            return std::stod(str);
        } catch (...) {
            return 0.0;
        }
    }

    static time_t ParseTimeArg(const std::string& str) {
        if (str.empty() || str == "-1" || str == "now") {
            return std::time(nullptr);
        }
        if (str == "-2") {
            return 0;
        }
        try {
            return static_cast<time_t>(std::stoll(str, nullptr, 0));
        } catch (...) {
            return std::time(nullptr);
        }
    }
};

class ArgumentQuoter {
public:
    static std::string QuoteArg(const std::string& arg) {
        if (arg.empty()) return "\"\"";
        bool needs_quotes = false;
        for (char c : arg) {
            if (std::isspace(static_cast<unsigned char>(c)) || c == '&' || c == '|' ||
                c == '<' || c == '>' || c == '^' || c == '"' || c == '%') {
                needs_quotes = true;
                break;
            }
        }
        if (!needs_quotes) return arg;

        std::string quoted = "\"";
        for (char c : arg) {
            if (c == '"') quoted += "\"\"";
            else quoted += c;
        }
        quoted += "\"";
        return quoted;
    }
};

// ============================================================================
// 2. FORMAT ENGINE
// ============================================================================

class FormatEngine {
public:
    static bool ProcessPercentB(const std::string& arg, std::ostream& out, bool& halt_output) {
        std::string result;
        size_t i = 0;
        while (i < arg.length()) {
            if (arg[i] == '\\') {
                i++;
                if (i >= arg.length()) {
                    result += '\\';
                    break;
                }
                if (arg[i] == 'c') {
                    halt_output = true;
                    out << result;
                    return false;
                }
                result += EscapeSequenceParser::ParseEscapeSequence(arg, i);
            } else {
                result += arg[i++];
            }
        }
        out << result;
        return true;
    }

    static void ProcessTimeFormat(const std::string& timeFmt, const std::string& arg_str, std::ostream& out) {
        time_t t = TypeCoercer::ParseTimeArg(arg_str);
        std::tm tm_buf;
        localtime_s(&tm_buf, &t);
        char buf[256];
        std::string actualFmt = timeFmt.empty() ? "%c" : timeFmt;
        size_t len = std::strftime(buf, sizeof(buf), actualFmt.c_str(), &tm_buf);
        if (len > 0) {
            out << buf;
        }
    }

    static void ProcessSpecifier(const std::string& fmt, size_t& i,
                                 const std::vector<std::string>& args, size_t& arg_index,
                                 std::ostream& out, bool& halt_output) {
        size_t start = i - 1;

        if (i < fmt.length() && fmt[i] == '(') {
            size_t closeParen = fmt.find(')', i);
            if (closeParen != std::string::npos && closeParen + 1 < fmt.length() && fmt[closeParen + 1] == 'T') {
                std::string datePattern = fmt.substr(i + 1, closeParen - (i + 1));
                i = closeParen + 2;
                std::string arg_str = (arg_index < args.size()) ? args[arg_index++] : "-1";
                ProcessTimeFormat(datePattern, arg_str, out);
                return;
            }
        }

        while (i < fmt.length() && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' || fmt[i] == '#' || fmt[i] == '0' || fmt[i] == '\'')) {
            i++;
        }

        bool width_from_arg = false;
        if (i < fmt.length() && fmt[i] == '*') {
            width_from_arg = true;
            i++;
        } else {
            while (i < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
                i++;
            }
        }

        bool precision_from_arg = false;
        if (i < fmt.length() && fmt[i] == '.') {
            i++;
            if (i < fmt.length() && fmt[i] == '*') {
                precision_from_arg = true;
                i++;
            } else {
                while (i < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
                    i++;
                }
            }
        }

        while (i < fmt.length() && (fmt[i] == 'h' || fmt[i] == 'l' || fmt[i] == 'L' || fmt[i] == 'z' || fmt[i] == 'j' || fmt[i] == 't')) {
            i++;
        }

        if (i >= fmt.length()) {
            out << '%';
            return;
        }

        char spec = fmt[i++];

        int star_width = 0;
        if (width_from_arg) {
            std::string w_str = (arg_index < args.size()) ? args[arg_index++] : "0";
            star_width = static_cast<int>(TypeCoercer::ParseIntArg(w_str));
        }

        int star_precision = 0;
        if (precision_from_arg) {
            std::string p_str = (arg_index < args.size()) ? args[arg_index++] : "0";
            star_precision = static_cast<int>(TypeCoercer::ParseIntArg(p_str));
        }

        std::string arg_str = (arg_index < args.size()) ? args[arg_index++] : "";
        std::string spec_str = fmt.substr(start, i - start);

        if (width_from_arg || precision_from_arg) {
            std::string rebuilt = "%";
            size_t p = start + 1;
            while (p < fmt.length() && (fmt[p] == '-' || fmt[p] == '+' || fmt[p] == ' ' || fmt[p] == '#' || fmt[p] == '0' || fmt[p] == '\'')) {
                rebuilt += fmt[p++];
            }
            if (width_from_arg) {
                rebuilt += std::to_string(star_width);
                p++;
            } else {
                while (p < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[p]))) {
                    rebuilt += fmt[p++];
                }
            }
            if (p < fmt.length() && fmt[p] == '.') {
                rebuilt += '.';
                p++;
                if (precision_from_arg) {
                    rebuilt += std::to_string(star_precision);
                    p++;
                } else {
                    while (p < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[p]))) {
                        rebuilt += fmt[p++];
                    }
                }
            }
            while (p < fmt.length() && (fmt[p] == 'h' || fmt[p] == 'l' || fmt[p] == 'L' || fmt[p] == 'z' || fmt[p] == 'j' || fmt[p] == 't')) {
                p++;
            }
            rebuilt += spec;
            spec_str = rebuilt;
        }

        if (spec == 'T') {
            ProcessTimeFormat("%c", arg_str, out);
            return;
        }

        if (spec == 'b') {
            ProcessPercentB(arg_str, out, halt_output);
            return;
        }

        if (spec == 'q') {
            std::string q = ArgumentQuoter::QuoteArg(arg_str);
            if (!spec_str.empty()) {
                spec_str.back() = 's';
            }
            char buf[2048];
            snprintf(buf, sizeof(buf), spec_str.c_str(), q.c_str());
            out << buf;
            return;
        }

        char buf[2048];
        if (spec == 'd' || spec == 'i') {
            spec_str.insert(spec_str.length() - 1, "ll");
            long long val = TypeCoercer::ParseIntArg(arg_str);
            snprintf(buf, sizeof(buf), spec_str.c_str(), val);
            out << buf;
        } else if (spec == 'o' || spec == 'u' || spec == 'x' || spec == 'X') {
            spec_str.insert(spec_str.length() - 1, "ll");
            unsigned long long val = static_cast<unsigned long long>(TypeCoercer::ParseIntArg(arg_str));
            snprintf(buf, sizeof(buf), spec_str.c_str(), val);
            out << buf;
        } else if (spec == 'f' || spec == 'F' || spec == 'e' || spec == 'E' || spec == 'g' || spec == 'G' || spec == 'a' || spec == 'A') {
            double val = TypeCoercer::ParseDoubleArg(arg_str);
            snprintf(buf, sizeof(buf), spec_str.c_str(), val);
            out << buf;
        } else if (spec == 's') {
            snprintf(buf, sizeof(buf), spec_str.c_str(), arg_str.c_str());
            out << buf;
        } else if (spec == 'c') {
            char c = arg_str.empty() ? '\0' : (arg_str[0] == '\'' || arg_str[0] == '"') && arg_str.length() > 1 ? arg_str[1] : arg_str[0];
            snprintf(buf, sizeof(buf), spec_str.c_str(), c);
            out << buf;
        } else if (spec == 'p') {
            unsigned long long val = static_cast<unsigned long long>(TypeCoercer::ParseIntArg(arg_str));
            snprintf(buf, sizeof(buf), spec_str.c_str(), reinterpret_cast<void*>(val));
            out << buf;
        } else {
            out << spec_str;
        }
    }

    static void ExecuteFormatLoop(const std::string& format, const std::vector<std::string>& args, std::ostream& out) {
        size_t arg_index = 0;
        bool halt_output = false;

        do {
            size_t prev_arg_index = arg_index;

            for (size_t i = 0; i < format.length(); ) {
                if (halt_output) break;

                if (format[i] == '\\') {
                    i++;
                    char c = EscapeSequenceParser::ParseEscapeSequence(format, i);
                    out << c;
                } else if (format[i] == '%') {
                    i++;
                    if (i < format.length() && format[i] == '%') {
                        out << '%';
                        i++;
                        continue;
                    }
                    ProcessSpecifier(format, i, args, arg_index, out, halt_output);
                } else {
                    out << format[i++];
                }
            }

            if (arg_index == prev_arg_index && arg_index >= args.size()) {
                break;
            }

        } while (arg_index < args.size() && !halt_output);
    }
};

// ============================================================================
// 3. OPTION PARSER & APPLICATION
// ============================================================================

struct PrintfOptions {
    bool show_help = false;
    bool show_version = false;
    std::string varName;
    std::string format;
    std::vector<std::string> args;
};

class OptionParser {
public:
    static void PrintHelp() {
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

    static void PrintVersion() {
        std::cout << "printf (CrossShell) 5.0.0\n";
    }

    bool Parse(int argc, char* argv[], PrintfOptions& opts) const {
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
};

class PrintfApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        PrintfOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            if (opts.show_help) {
                OptionParser::PrintHelp();
                return 0;
            }
            if (opts.show_version) {
                OptionParser::PrintVersion();
                return 0;
            }
            std::cerr << "usage: printf [-v var] format [argument ...]\n";
            return 1;
        }

        if (opts.show_help) {
            OptionParser::PrintHelp();
            return 0;
        }

        if (opts.show_version) {
            OptionParser::PrintVersion();
            return 0;
        }

        if (!opts.varName.empty()) {
            std::ostringstream ss;
            FormatEngine::ExecuteFormatLoop(opts.format, opts.args, ss);
            SetEnvironmentVariableA(opts.varName.c_str(), ss.str().c_str());
        } else {
            FormatEngine::ExecuteFormatLoop(opts.format, opts.args, std::cout);
        }
        return 0;
    }
};

int main(int argc, char* argv[]) {
    PrintfApplication app;
    return app.Run(argc, argv);
}