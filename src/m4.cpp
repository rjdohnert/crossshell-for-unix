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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cctype>
#include <cstdio>
#include <streambuf>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// Class to manage pushbacks (LIFO buffer) and file inclusions
class PushbackStream {
private:
    std::vector<std::istream*> stream_stack;
    std::string pushback_buf;

public:
    PushbackStream() {}

    void push_stream(std::istream* stream) {
        stream_stack.push_back(stream);
    }

    void pop_stream() {
        if (!stream_stack.empty()) {
            stream_stack.pop_back();
        }
    }

    // Pushback a string (reverses character order to preserve LIFO reading)
    void push_back(const std::string& s) {
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            pushback_buf.push_back(*it);
        }
    }

    void push_char(char c) {
        pushback_buf.push_back(c);
    }

    int get_char() {
        if (!pushback_buf.empty()) {
            char c = pushback_buf.back();
            pushback_buf.pop_back();
            return static_cast<unsigned char>(c);
        }
        while (!stream_stack.empty()) {
            std::istream* current = stream_stack.back();
            int c = current->get();
            if (c != EOF) {
                return c;
            }
            if (stream_stack.size() > 1) {
                stream_stack.pop_back();
            } else {
                return EOF;
            }
        }
        return EOF;
    }
};

enum class SyntaxTokenType {
    EOF_TOKEN,
    NAME,
    SIMPLE,
    QUOTED,
    COMMENT
};

struct Token {
    SyntaxTokenType type;
    std::string value;
};

enum class OutputFormat { Human, Json, Csv, Table };

class PipeStreambuf : public std::streambuf {
private:
    FILE* pipe_;
    char buffer_[4096];
protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) { *pptr() = static_cast<char>(ch); pbump(1); }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }
    int sync() override {
        std::ptrdiff_t count = pptr() - pbase();
        if (count > 0 && std::fwrite(pbase(), 1, static_cast<size_t>(count), pipe_) != static_cast<size_t>(count)) return -1;
        setp(buffer_, buffer_ + sizeof(buffer_));
        return std::fflush(pipe_) == 0 ? 0 : -1;
    }
public:
    explicit PipeStreambuf(FILE* pipe) : pipe_(pipe) { setp(buffer_, buffer_ + sizeof(buffer_)); }
    ~PipeStreambuf() override { sync(); }
};

// ============================================================================
// 1. OUTPUT FORMATTING HELPERS
// ============================================================================
class OutputFormatter {
public:
    static std::string JsonEscape(const std::string& value) {
        std::string out;
        for (unsigned char ch : value) {
            switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch < 0x20 ? '?' : static_cast<char>(ch)); break;
            }
        }
        return out;
    }

    static std::string CsvEscape(const std::string& value) {
        std::string out = "\"";
        for (char ch : value) {
            if (ch == '"') out += "\"\"";
            else out.push_back(ch);
        }
        return out + "\"";
    }
};

// ============================================================================
// 2. M4 MACRO PROCESSOR ENGINE
// ============================================================================
class M4Processor {
private:
    PushbackStream input;
    std::string lquote = "`";
    std::string rquote = "'";
    std::string comment_start = "#";
    std::string comment_end = "\n";
    std::ostream& out_stream;
    std::vector<std::string> include_paths;
    OutputFormat output_format = OutputFormat::Human;
    bool json_first = true;

    struct MacroDef {
        bool is_builtin;
        std::string text;
        std::function<std::string(const std::vector<std::string>&)> builtin_func;
    };

    std::unordered_map<std::string, MacroDef> macros;

    // Helper to match a multi-character delimiter at the current stream position
    bool match_string(int first_char, const std::string& pattern) {
        if (pattern.empty()) return false;
        if (first_char != static_cast<unsigned char>(pattern[0])) return false;

        std::string read_chars;
        read_chars.push_back(static_cast<char>(first_char));
        bool matched = true;

        for (size_t i = 1; i < pattern.size(); ++i) {
            int next = input.get_char();
            if (next == EOF) {
                matched = false;
                break;
            }
            read_chars.push_back(static_cast<char>(next));
            if (next != static_cast<unsigned char>(pattern[i])) {
                matched = false;
                break;
            }
        }

        if (!matched) {
            input.push_back(read_chars);
            return false;
        }
        return true;
    }

    Token get_token() {
        int c = input.get_char();
        if (c == EOF) {
            return {SyntaxTokenType::EOF_TOKEN, ""};
        }

        // Handle Comments
        if (!comment_start.empty() && match_string(c, comment_start)) {
            std::string comment = comment_start;
            while (true) {
                int next_c = input.get_char();
                if (next_c == EOF) break;
                comment.push_back(static_cast<char>(next_c));
                if (!comment_end.empty() && comment.size() >= comment_end.size() &&
                    comment.compare(comment.size() - comment_end.size(), comment_end.size(), comment_end) == 0) {
                    break;
                }
            }
            return {SyntaxTokenType::COMMENT, comment};
        }

        // Handle Quoting
        if (!lquote.empty() && match_string(c, lquote)) {
            int nest_level = 1;
            std::string content;
            while (true) {
                int next_c = input.get_char();
                if (next_c == EOF) break;

                if (!lquote.empty() && match_string(next_c, lquote)) {
                    nest_level++;
                    content += lquote;
                } else if (!rquote.empty() && match_string(next_c, rquote)) {
                    nest_level--;
                    if (nest_level == 0) break;
                    content += rquote;
                } else {
                    content.push_back(static_cast<char>(next_c));
                }
            }
            return {SyntaxTokenType::QUOTED, content};
        }

        // Handle Alphanumeric macro names
        if (std::isalpha(c) || c == '_') {
            std::string name;
            name.push_back(static_cast<char>(c));
            while (true) {
                int next_c = input.get_char();
                if (next_c == EOF) break;
                if (std::isalnum(next_c) || next_c == '_') {
                    name.push_back(static_cast<char>(next_c));
                } else {
                    input.push_char(static_cast<char>(next_c));
                    break;
                }
            }
            return {SyntaxTokenType::NAME, name};
        }

        // Simple raw character
        return {SyntaxTokenType::SIMPLE, std::string(1, static_cast<char>(c))};
    }

    void push_token_back(const Token& t) {
        if (t.type == SyntaxTokenType::QUOTED) {
            input.push_back(lquote + t.value + rquote);
        } else {
            input.push_back(t.value);
        }
    }

    // Collect macro arguments and respect nested quoting
    std::vector<std::string> collect_arguments() {
        std::vector<std::string> args;
        Token t = get_token();
        if (t.type != SyntaxTokenType::SIMPLE || t.value != "(") {
            if (t.type != SyntaxTokenType::EOF_TOKEN) {
                push_token_back(t);
            }
            return args;
        }

        std::string current_arg;
        int paren_depth = 0;
        while (true) {
            Token arg_t = get_token();
            if (arg_t.type == SyntaxTokenType::EOF_TOKEN) break;

            if (arg_t.type == SyntaxTokenType::SIMPLE) {
                if (arg_t.value == "(") {
                    paren_depth++;
                    current_arg += "(";
                } else if (arg_t.value == ")") {
                    if (paren_depth == 0) {
                        args.push_back(current_arg);
                        break;
                    } else {
                        paren_depth--;
                        current_arg += ")";
                    }
                } else if (arg_t.value == "," && paren_depth == 0) {
                    args.push_back(current_arg);
                    current_arg.clear();
                } else {
                    current_arg += arg_t.value;
                }
            } else if (arg_t.type == SyntaxTokenType::QUOTED) {
                current_arg += lquote + arg_t.value + rquote;
            } else if (arg_t.type == SyntaxTokenType::NAME) {
                if (macros.count(arg_t.value)) {
                    expand_macro(arg_t.value);
                } else {
                    current_arg += arg_t.value;
                }
            } else {
                current_arg += arg_t.value;
            }
        }
        return args;
    }

    // Standard $1, $2, $# argument substitution
    std::string substitute_args(const std::string& def, const std::vector<std::string>& args) {
        std::string res;
        for (size_t i = 0; i < def.size(); ++i) {
            if (def[i] == '$') {
                if (i + 1 < def.size()) {
                    char next = def[i + 1];
                    if (std::isdigit(next)) {
                        int arg_idx = next - '0';
                        if (arg_idx < static_cast<int>(args.size())) {
                            res += args[arg_idx];
                        }
                        i++;
                    } else if (next == '#') {
                        int count = static_cast<int>(args.size()) - 1;
                        res += std::to_string(count < 0 ? 0 : count);
                        i++;
                    } else if (next == '*') {
                        for (size_t j = 1; j < args.size(); ++j) {
                            if (j > 1) res += ",";
                            res += args[j];
                        }
                        i++;
                    } else if (next == '@') {
                        for (size_t j = 1; j < args.size(); ++j) {
                            if (j > 1) res += ",";
                            res += lquote + args[j] + rquote;
                        }
                        i++;
                    } else {
                        res.push_back('$');
                    }
                } else {
                    res.push_back('$');
                }
            } else {
                res.push_back(def[i]);
            }
        }
        return res;
    }

    void register_builtins() {
        macros["define"] = { true, "", [this](const std::vector<std::string>& args) {
            if (args.size() > 2) {
                macros[args[1]] = { false, args[2], nullptr };
            }
            return "";
        }};

        macros["undefine"] = { true, "", [this](const std::vector<std::string>& args) {
            if (args.size() > 1) {
                macros.erase(args[1]);
            }
            return "";
        }};

        macros["ifdef"] = { true, "", [this](const std::vector<std::string>& args) -> std::string {
            if (args.size() > 2) {
                if (macros.count(args[1]) > 0) {
                    return args[2];
                } else if (args.size() > 3) {
                    return args[3];
                }
            }
            return std::string("");
        }};

        macros["ifelse"] = { true, "", [this](const std::vector<std::string>& args) {
            if (args.size() < 4) return std::string("");
            size_t i = 1;
            while (i + 2 < args.size()) {
                if (args[i] == args[i + 1]) {
                    return args[i + 2];
                }
                i += 3;
            }
            if (i < args.size()) return args[i];
            return std::string("");
        }};

        macros["dnl"] = { true, "", [this](const std::vector<std::string>&) {
            while (true) {
                int c = input.get_char();
                if (c == EOF || c == '\n') break;
            }
            return "";
        }};

        macros["len"] = { true, "", [](const std::vector<std::string>& args) {
            return args.size() > 1 ? std::to_string(args[1].size()) : "0";
        }};

        macros["changequote"] = { true, "", [this](const std::vector<std::string>& args) {
            if (args.size() > 2) {
                lquote = args[1];
                rquote = args[2];
            } else {
                lquote = "`";
                rquote = "'";
            }
            return "";
        }};

        macros["include"] = { true, "", [this](const std::vector<std::string>& args) -> std::string {
            if (args.size() > 1) {
                std::string resolved = resolve_include_path(args[1]);
                if (!resolved.empty()) {
                    std::ifstream file(resolved, std::ios::binary);
                    if (file.is_open()) {
                        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    }
                }
                std::cerr << "m4: cannot open '" << args[1] << "': No such file or directory\n";
            }
            return std::string("");
        }};
    }

public:
    explicit M4Processor(std::ostream& out) : out_stream(out) {
        register_builtins();
    }

    void define_macro(const std::string& name, const std::string& value = "") {
        if (!name.empty()) {
            macros[name] = { false, value, nullptr };
        }
    }

    void undefine_macro(const std::string& name) {
        if (!name.empty()) {
            macros.erase(name);
        }
    }

    void add_include_path(const std::string& path) {
        if (!path.empty()) {
            include_paths.push_back(path);
        }
    }

    std::string resolve_include_path(const std::string& path) {
        std::ifstream direct(path, std::ios::binary);
        if (direct.is_open()) {
            return path;
        }

        for (const std::string& dir : include_paths) {
            std::string candidate = dir;
            if (!candidate.empty() && candidate.back() != '/' && candidate.back() != '\\') {
                candidate += "/";
            }
            candidate += path;
            std::ifstream file(candidate, std::ios::binary);
            if (file.is_open()) {
                return candidate;
            }
        }
        return "";
    }

    void set_output_format(OutputFormat format) { output_format = format; }

    void emit(const std::string& value) {
        if (output_format == OutputFormat::Json) {
            if (!json_first) out_stream << ",\n";
            json_first = false;
            out_stream << "{\"value\":\"" << OutputFormatter::JsonEscape(value) << "\"}";
        } else if (output_format == OutputFormat::Csv) {
            out_stream << OutputFormatter::CsvEscape(value) << "\n";
        } else if (output_format == OutputFormat::Table) {
            out_stream << value << "\n";
        } else {
            out_stream << value;
        }
    }

    void expand_macro(const std::string& name) {
        auto it = macros.find(name);
        if (it == macros.end()) return;

        std::vector<std::string> args = { name };
        std::vector<std::string> collected = collect_arguments();
        args.insert(args.end(), collected.begin(), collected.end());

        std::string expansion;
        if (it->second.is_builtin) {
            expansion = it->second.builtin_func(args);
        } else {
            expansion = substitute_args(it->second.text, args);
        }

        input.push_back(expansion);
    }

    void process(std::istream& in) {
        input.push_stream(&in);
        while (true) {
            Token t = get_token();
            if (t.type == SyntaxTokenType::EOF_TOKEN) break;

            if (t.type == SyntaxTokenType::NAME) {
                if (macros.count(t.value)) {
                    expand_macro(t.value);
                } else {
                    emit(t.value);
                }
            } else {
                emit(t.value);
            }
        }
        input.pop_stream();
    }
};

// ============================================================================
// 3. COMMAND LINE OPTION PARSER
// ============================================================================
struct M4Options {
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Human;
    std::string pipeCommand;
    std::vector<std::pair<std::string, std::string>> definitions;
    std::vector<std::string> undefinitions;
    std::vector<std::string> includePaths;
    std::vector<std::string> inputFiles;
};

class OptionParser {
public:
    static void PrintUsage(const std::string& /*progName*/ = "m4") {
        std::cout << R"(m4(1)                    CrossShell for UNIX Reference Manual                   m4(1)

    NAME
        m4 - process macros in files or standard input

    SYNOPSIS
        m4 [OPTIONS] [FILE]...

    DESCRIPTION
        Expands macros in input files or standard input. Built-in macros include
        define, undefine, ifdef, ifelse, dnl, len, changequote, and include.
        Definitions support positional arguments, argument counts, and quoted or
        unquoted argument expansion.

    OPTIONS
        -D, --define <name>[=<value>]
            Define NAME as VALUE; VALUE defaults to empty.

        -U, --undefine <name>
            Remove a macro definition.

        -I, --include <dir>
            Add DIRECTORY to the include search path.

        --json
            Emit expanded chunks as a JSON array.

        --csv
            Emit expanded chunks as CSV records.

        --table
            Emit expanded chunks one per line with a VALUE header.

        --pipe <command>
            Send formatted output through COMMAND.

        -
            Read standard input.

        -h, --help
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

        --
            End options; remaining arguments are input files.

    INPUT AND OUTPUT
        include searches the direct path first, then configured -I directories.
        JSON objects contain a value field. CSV emits one quoted expansion per
        line. A file argument is read as binary input; '-' reads standard input.

    EXAMPLES
        m4 input.m4
            Expand macros in a file.

        m4 -DNAME=World template.m4
            Define a macro before processing the template.

        m4 -I include --json input.m4
            Search an include directory and emit JSON.

        type input.m4 | m4 - --table
            Process standard input and emit one expanded chunk per line.

    EXIT STATUS
        0
            Help, version, or successful macro processing.
        1
            Unknown option, missing argument, input failure, or pipe failure.

    CrossShell for UNIX                                                       m4(1)
)";
    }

    static void PrintVersion() {
        std::cout << "m4 17.8\n";
    }

    bool Parse(int argc, char* argv[], M4Options& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    opts.inputFiles.push_back(argv[i]);
                }
                break;
            } else if (arg == "-h" || arg == "--help") {
                opts.showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                opts.showVersion = true;
                return true;
            } else if (arg == "--json") {
                opts.format = OutputFormat::Json;
            } else if (arg == "--csv") {
                opts.format = OutputFormat::Csv;
            } else if (arg == "--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == "--pipe") {
                if (i + 1 >= argc) {
                    std::cerr << "m4: option requires an argument -- 'pipe'\n";
                    return false;
                }
                opts.pipeCommand = argv[++i];
            } else if (arg == "-D" || arg == "--define") {
                if (i + 1 < argc) {
                    AddDefinition(argv[++i], opts);
                } else {
                    std::cerr << "m4: option requires an argument -- 'D'\n";
                    return false;
                }
            } else if (arg.rfind("--define=", 0) == 0) {
                AddDefinition(arg.substr(9), opts);
            } else if (arg.rfind("-D", 0) == 0 && arg.size() > 2) {
                AddDefinition(arg.substr(2), opts);
            } else if (arg == "-U" || arg == "--undefine") {
                if (i + 1 < argc) {
                    opts.undefinitions.push_back(argv[++i]);
                } else {
                    std::cerr << "m4: option requires an argument -- 'U'\n";
                    return false;
                }
            } else if (arg.rfind("--undefine=", 0) == 0) {
                opts.undefinitions.push_back(arg.substr(11));
            } else if (arg.rfind("-U", 0) == 0 && arg.size() > 2) {
                opts.undefinitions.push_back(arg.substr(2));
            } else if (arg == "-I" || arg == "--include") {
                if (i + 1 < argc) {
                    opts.includePaths.push_back(argv[++i]);
                } else {
                    std::cerr << "m4: option requires an argument -- 'I'\n";
                    return false;
                }
            } else if (arg.rfind("--include=", 0) == 0) {
                opts.includePaths.push_back(arg.substr(10));
            } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
                opts.includePaths.push_back(arg.substr(2));
            } else if (arg == "-") {
                opts.inputFiles.push_back(arg);
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "m4: unknown option: " << arg << "\n";
                return false;
            } else {
                opts.inputFiles.push_back(arg);
            }
        }
        return true;
    }

private:
    static void AddDefinition(const std::string& spec, M4Options& opts) {
        size_t eq = spec.find('=');
        std::string name = spec.substr(0, eq);
        std::string value = (eq == std::string::npos) ? "" : spec.substr(eq + 1);
        opts.definitions.push_back({name, value});
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================
class M4Application {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(NULL);

#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        M4Options options;
        if (!m_parser.Parse(argc, argv, options)) {
            return 1;
        }

        std::string progName = (argc > 0) ? argv[0] : "m4";

        if (options.showHelp) {
            OptionParser::PrintUsage(progName);
            return 0;
        }

        if (options.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        M4Processor processor(std::cout);
        for (const auto& def : options.definitions) {
            processor.define_macro(def.first, def.second);
        }
        for (const auto& undef : options.undefinitions) {
            processor.undefine_macro(undef);
        }
        for (const auto& inc : options.includePaths) {
            processor.add_include_path(inc);
        }
        processor.set_output_format(options.format);

        FILE* output_pipe = nullptr;
        PipeStreambuf* pipe_buffer = nullptr;
        std::streambuf* old_output_buffer = nullptr;

        if (!options.pipeCommand.empty()) {
            output_pipe = _popen(options.pipeCommand.c_str(), "w");
            if (!output_pipe) {
                std::cerr << "m4: failed to start pipe command\n";
                return 1;
            }
            old_output_buffer = std::cout.rdbuf();
            pipe_buffer = new PipeStreambuf(output_pipe);
            std::cout.rdbuf(pipe_buffer);
        }

        if (options.format == OutputFormat::Json) std::cout << "[\n";
        if (options.format == OutputFormat::Table) std::cout << "VALUE\n-----\n";

        if (options.inputFiles.empty()) {
            processor.process(std::cin);
        } else {
            for (const auto& file : options.inputFiles) {
                if (file == "-") {
                    processor.process(std::cin);
                } else {
                    std::ifstream infile(file, std::ios::binary);
                    if (!infile.is_open()) {
                        std::cerr << "m4: cannot open '" << file << "': No such file or directory\n";
                        if (pipe_buffer) {
                            std::cout.rdbuf(old_output_buffer);
                            delete pipe_buffer;
                            _pclose(output_pipe);
                        }
                        return 1;
                    }
                    processor.process(infile);
                }
            }
        }

        if (options.format == OutputFormat::Json) std::cout << "\n]\n";

        if (pipe_buffer) {
            std::cout.rdbuf(old_output_buffer);
            delete pipe_buffer;
            _pclose(output_pipe);
        }

        return 0;
    }
};

// ============================================================================
// 5. ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    M4Application app;
    return app.Run(argc, argv);
}
