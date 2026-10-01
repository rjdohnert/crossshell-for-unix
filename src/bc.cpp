/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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
#include <string>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <vector>

class ExpressionParser {
private:
    std::string src;
    size_t pos = 0;

    char peek() const {
        if (pos < src.length()) return src[pos];
        return '\0';
    }

    char get() {
        if (pos < src.length()) return src[pos++];
        return '\0';
    }

    void skip_whitespace() {
        while (pos < src.length() && std::isspace(static_cast<unsigned char>(src[pos]))) {
            pos++;
        }
    }

    double expression();
    double term();
    double factor();
    double primary();
    double number();
    std::string name();

public:
    explicit ExpressionParser(std::string str) : src(std::move(str)), pos(0) {}

    double parse() {
        pos = 0;
        double res = expression();
        skip_whitespace();
        if (pos < src.length()) {
            throw std::runtime_error("Unexpected character: '" + std::string(1, src[pos]) + "'");
        }
        return res;
    }
};

// Handles addition and subtraction
double ExpressionParser::expression() {
    double left = term();
    while (true) {
        skip_whitespace();
        char op = peek();
        if (op == '+' || op == '-') {
            get();
            double right = term();
            if (op == '+') left += right;
            else left -= right;
        } else {
            break;
        }
    }
    return left;
}

// Handles multiplication, division, and modulo
double ExpressionParser::term() {
    double left = factor();
    while (true) {
        skip_whitespace();
        char op = peek();
        if (op == '*' || op == '/' || op == '%') {
            get();
            double right = factor();
            if (op == '*') {
                left *= right;
            } else if (op == '/') {
                if (right == 0.0) throw std::runtime_error("Division by zero");
                left /= right;
            } else {
                if (right == 0.0) throw std::runtime_error("Modulo by zero");
                left = std::fmod(left, right);
            }
        } else {
            break;
        }
    }
    return left;
}

// Handles exponentiation (right-associative)
double ExpressionParser::factor() {
    double left = primary();
    skip_whitespace();
    if (peek() == '^') {
        get();
        double right = factor();
        left = std::pow(left, right);
    }
    return left;
}

// Handles numbers, parentheses, functions, and unary operators
double ExpressionParser::primary() {
    skip_whitespace();
    char c = peek();
    if (c == '\0') {
        throw std::runtime_error("Unexpected end of expression");
    }

    // Unary plus and minus
    if (c == '+') {
        get();
        return primary();
    }
    if (c == '-') {
        get();
        return -primary();
    }

    // Parentheses
    if (c == '(') {
        get();
        double val = expression();
        skip_whitespace();
        if (get() != ')') {
            throw std::runtime_error("Missing closing parenthesis");
        }
        return val;
    }

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
        return number();
    }

    // Functions and Constants
    if (std::isalpha(static_cast<unsigned char>(c))) {
        std::string id = name();
        skip_whitespace();
        
        if (peek() == '(') {
            get(); // consume '('
            double arg = expression();
            skip_whitespace();
            if (get() != ')') {
                throw std::runtime_error("Missing closing parenthesis for function '" + id + "'");
            }
            
            // Normalize function names to lowercase
            std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c){ return std::tolower(c); });

            if (id == "sin") return std::sin(arg);
            if (id == "cos") return std::cos(arg);
            if (id == "tan") return std::tan(arg);
            if (id == "asin") return std::asin(arg);
            if (id == "acos") return std::acos(arg);
            if (id == "atan") return std::atan(arg);
            if (id == "sqrt") {
                if (arg < 0) throw std::runtime_error("Square root of negative number");
                return std::sqrt(arg);
            }
            if (id == "log") {
                if (arg <= 0) throw std::runtime_error("Logarithm (base 10) of non-positive number");
                return std::log10(arg);
            }
            if (id == "ln") {
                if (arg <= 0) throw std::runtime_error("Natural logarithm of non-positive number");
                return std::log(arg);
            }
            if (id == "abs") return std::abs(arg);
            if (id == "exp") return std::exp(arg);

            throw std::runtime_error("Unknown function: '" + id + "'");
        } else {
            // Check for constants
            std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c){ return std::tolower(c); });
            if (id == "pi") return 3.14159265358979323846;
            if (id == "e") return 2.71828182845904523536;
            throw std::runtime_error("Unknown constant: '" + id + "'");
        }
    }

    throw std::runtime_error("Unexpected character: '" + std::string(1, c) + "'");
}

double ExpressionParser::number() {
    size_t start = pos;
    bool has_dot = false;
    bool has_e = false;

    while (pos < src.length()) {
        char c = src[pos];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            pos++;
        } else if (c == '.' && !has_dot && !has_e) {
            has_dot = true;
            pos++;
        } else if ((c == 'e' || c == 'E') && !has_e) {
            has_e = true;
            pos++;
            if (pos < src.length() && (src[pos] == '+' || src[pos] == '-')) {
                pos++;
            }
        } else {
            break;
        }
    }
    if (start == pos) {
        throw std::runtime_error("Expected a number");
    }
    return std::stod(src.substr(start, pos - start));
}

std::string ExpressionParser::name() {
    size_t start = pos;
    while (pos < src.length() && std::isalpha(static_cast<unsigned char>(src[pos]))) {
        pos++;
    }
    return src.substr(start, pos - start);
}

struct BcOptions {
    int output_format = 0;
    std::string pipe_command;
};

static std::string json_escape(const std::string& value) {
    std::string result;
    for (char ch : value) {
        if (ch == '\\') result += "\\\\";
        else if (ch == '"') result += "\\\"";
        else if (ch == '\n') result += "\\n";
        else if (ch == '\r') result += "\\r";
        else if (ch == '\t') result += "\\t";
        else result += ch;
    }
    return result;
}

static std::string csv_escape(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        if (ch == '\"') result += "\"\"";
        else result += ch;
    }
    result += '\"';
    return result;
}

static void emit_result(const std::string& expression, double result, const BcOptions& options) {
    std::ostringstream value;
    value << result;
    std::string output;
    if (options.output_format == 1) {
        output = "{\"expression\":\"" + json_escape(expression) + "\",\"result\":" + value.str() + "}\n";
    } else if (options.output_format == 2) {
        output = "expression,result\n" + csv_escape(expression) + "," + value.str() + "\n";
    } else if (options.output_format == 3) {
        output = "EXPRESSION\tRESULT\n" + expression + "\t" + value.str() + "\n";
    } else {
        output = value.str() + "\n";
    }

    if (!options.pipe_command.empty()) {
        FILE* pipe = _popen(options.pipe_command.c_str(), "w");
        if (!pipe) throw std::runtime_error("cannot open pipe command");
        fwrite(output.data(), 1, output.size(), pipe);
        _pclose(pipe);
    } else {
        std::cout << output;
    }
}

void show_help() {
    std::cout << R"HELP(bc(1)                  CrossShell for UNIX Reference Manual                   bc(1)

    NAME
        bc - Evaluate mathematical expressions interactively or from scripts.

    SYNOPSIS
        bc [OPTIONS]
        bc [OPTIONS] EXPRESSION
        bc [OPTIONS] -- EXPRESSION...

    DESCRIPTION
        Evaluates double-precision mathematical expressions supplied on the
        command line or through standard input. With no expression, bc starts
        an interactive session. Results can be formatted for scripts or sent
        through another command using a Windows pipe.

    OPTIONS
        --json
            Format each result as a JSON object.

        --csv
            Format each result as CSV with expression and result columns.

        --table
            Format each result as a two-column table.

        --pipe COMMAND
            Send result output through COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Display version information.

    OPERATORS AND FUNCTIONS
        +, -, *, /, %, ^
            Arithmetic operators. The ^ operator performs exponentiation.

        ( expression )
            Group an expression to control evaluation order.

        pi, e
            Built-in mathematical constants.

        sin, cos, tan, asin, acos, atan
            Trigonometric functions. Angles are measured in radians.

        sqrt, log, ln, abs, exp
            Square root, base-10 logarithm, natural logarithm, absolute
            value, and exponential functions.

    INTERACTIVE COMMANDS
        help
            Display this reference manual.

        exit, quit
            End the interactive session.

    EXAMPLES
        bc "2 + 3 * 4"
            Evaluate one command-line expression.

        bc -- 2 + 3 * 4
            Join the remaining arguments and evaluate them as one expression.

        bc --json "sqrt(81)"
            Format the result as JSON.

        bc --table --pipe "more" "2 ^ 16"
            Send a tabular result through another command.

    CrossShell for UNIX                                                        bc(1)
    )HELP";
}

void show_version() {
    std::cout << "bc (IBM AIX 7.3)\n";
}

int evaluate_expression(const std::string& input, const BcOptions& options) {
    try {
        ExpressionParser parser(input);
        double result = parser.parse();
        emit_result(input, result, options);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

class BcApplication {
public:
    static int run(int argc, char** argv) {
        BcOptions options;
        std::vector<std::string> expressions;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") options.output_format = 1;
            else if (arg == "--csv") options.output_format = 2;
            else if (arg == "--table") options.output_format = 3;
            else if (arg == "--pipe" && i + 1 < argc) options.pipe_command = argv[++i];
            else expressions.push_back(arg);
        }

        if (!expressions.empty()) {
            std::string arg = expressions.front();
        if (arg == "--help" || arg == "-h") {
            show_help();
            return 0;
        }
        if (arg == "--version") {
            show_version();
            return 0;
        }
        if (arg == "--") {
            if (expressions.size() > 1) {
                std::string expr;
                for (size_t i = 1; i < expressions.size(); ++i) {
                    if (i > 1) expr += ' ';
                    expr += expressions[i];
                }
                return evaluate_expression(expr, options);
            }
            return 0;
        }
        if (expressions.size() == 1) {
            return evaluate_expression(arg, options);
        }
        }

    show_help();
    std::string input;

    while (true) {
        if (options.output_format == 0 && options.pipe_command.empty()) std::cout << "bc> ";
        if (!std::getline(std::cin, input)) {
            break;
        }

        std::string command = input;
        command.erase(command.begin(), std::find_if(command.begin(), command.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        command.erase(std::find_if(command.rbegin(), command.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), command.end());

        if (command.empty()) {
            continue;
        }

        if (command == "exit" || command == "quit") {
            break;
        }

        if (command == "help") {
            show_help();
            continue;
        }

        evaluate_expression(input, options);
    }

        return 0;
    }
};

int main(int argc, char** argv) {
    return BcApplication::run(argc, argv);
}
