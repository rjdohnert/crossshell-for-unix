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
#include <vector>
#include <string>
#include <memory>
#include <regex>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <climits>
#include <system_error>

namespace fs = std::filesystem;

std::string pathToUtf8(const fs::path& p) {
#if defined(_WIN32)
    auto u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
#else
    return p.string();
#endif
}

void PrintUsage() {
    std::cout << R"(locate(1)         CrossShell for UNIX Reference Manual                locate(1)

    NAME
        locate - search for files and directories using expression filters

    SYNOPSIS
        locate [OPTIONS] [PATH...] [EXPRESSION...]

    DESCRIPTION
        locate searches directory trees for files and directories matching
        specified criteria and expressions, executing actions such as printing
        or deletion. It supports predicate combinations, depth limits, and
        pattern filtering.

    OPTIONS
        -L, --follow
            Follow symbolic links during recursion.

        -P, --no-follow
            Do not follow symbolic links (default).

        --maxdepth N
            Limit search recursion depth to N levels.

        --mindepth N
            Do not apply tests or actions at levels less than N.

        --name PATTERN
            Match base filename against shell glob pattern.

        --path PATTERN
            Match full relative file path against pattern.

        --type TYPE
            Match file type: f (file), d (directory), l (symlink).

        --size N
            Match file size in bytes or scaled units.

        --mtime N
            Match file modification time in days.

        --mmin N
            Match file modification time in minutes.

        --empty
            Match empty files or empty directories.

        --print
            Print matched paths followed by newline (default).

        --print0
            Print matched paths followed by NUL character.

        --delete
            Delete matched files or directories.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        locate . --name "*.cpp"
            Find all C++ source files under current directory.

        locate C:\Projects --type f --size +10M
            Find files larger than 10 MB.

        locate . --maxdepth 2 --type d
            List directories up to 2 levels deep.

        locate src/ --name "*.tmp" --delete
            Find and delete temporary files in src/.

    CrossShell for UNIX                                                   locate(1)
)";
}

void PrintVersion() {
    std::cout << "locate v1.0.0\n";
}

std::string CanonicalizeOption(const std::string& arg) {
    if (arg == "--name") return "-name";
    if (arg == "--iname") return "-iname";
    if (arg == "--path") return "-path";
    if (arg == "--ipath") return "-ipath";
    if (arg == "--type") return "-type";
    if (arg == "--size") return "-size";
    if (arg == "--mtime") return "-mtime";
    if (arg == "--mmin") return "-mmin";
    if (arg == "--empty") return "-empty";
    if (arg == "--maxdepth") return "-maxdepth";
    if (arg == "--mindepth") return "-mindepth";
    if (arg == "--print") return "-print";
    if (arg == "--print0") return "-print0";
    if (arg == "--delete") return "-delete";
    if (arg == "--not") return "-not";
    if (arg == "--and") return "-and";
    if (arg == "--or") return "-or";
    return arg;
}

// Context passed to expression evaluators
struct FindContext {
    fs::directory_entry entry;
    int depth;
    fs::path start_path;
};

// Abstract Base Expression Node
class Expression {
public:
    virtual ~Expression() = default;
    virtual bool evaluate(const FindContext& ctx) = 0;
    virtual bool is_action() const { return false; }
};

// Convert shell wildcard pattern (* and ?) into std::regex
std::regex wildcard_to_regex(const std::string& pattern, bool case_insensitive) {
    std::string rx = "^";
    for (char c : pattern) {
        switch (c) {
            case '*': rx += ".*"; break;
            case '?': rx += "."; break;
            case '.': case '+': case '^': case '$': case '(': case ')':
            case '[': case ']': case '{': case '}': case '|': case '\\':
                rx += '\\'; rx += c; break;
            default:
                rx += c; break;
        }
    }
    rx += "$";
    auto flags = std::regex::ECMAScript;
    if (case_insensitive) flags |= std::regex::icase;
    return std::regex(rx, flags);
}

// Logical AND Expression
class AndExpr : public Expression {
    std::shared_ptr<Expression> left, right;
public:
    AndExpr(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r) : left(l), right(r) {}
    bool evaluate(const FindContext& ctx) override {
        if (!left || !left->evaluate(ctx)) return false;
        if (!right) return true;
        return right->evaluate(ctx);
    }
};

// Logical OR Expression
class OrExpr : public Expression {
    std::shared_ptr<Expression> left, right;
public:
    OrExpr(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r) : left(l), right(r) {}
    bool evaluate(const FindContext& ctx) override {
        if (left && left->evaluate(ctx)) return true;
        if (right && right->evaluate(ctx)) return true;
        return false;
    }
};

// Logical NOT Expression
class NotExpr : public Expression {
    std::shared_ptr<Expression> child;
public:
    NotExpr(std::shared_ptr<Expression> c) : child(c) {}
    bool evaluate(const FindContext& ctx) override {
        return child ? !child->evaluate(ctx) : false;
    }
};

class TrueExpr : public Expression {
public:
    bool evaluate(const FindContext&) override { return true; }
};

// -name / -iname Selector
class NameExpr : public Expression {
    std::regex rx;
public:
    NameExpr(const std::string& pattern, bool icase)
        : rx(wildcard_to_regex(pattern, icase)) {}

    bool evaluate(const FindContext& ctx) override {
        std::string filename = pathToUtf8(ctx.entry.path().filename());
        return std::regex_match(filename, rx);
    }
};

// -path / -ipath Selector
class PathExpr : public Expression {
    std::regex rx;
public:
    PathExpr(const std::string& pattern, bool icase)
        : rx(wildcard_to_regex(pattern, icase)) {}

    bool evaluate(const FindContext& ctx) override {
        fs::path p = ctx.entry.path();
        p.make_preferred();
        return std::regex_match(pathToUtf8(p), rx);
    }
};

// -type [f|d|l] Selector
class TypeExpr : public Expression {
    char type;
public:
    TypeExpr(char t) : type(t) {}

    bool evaluate(const FindContext& ctx) override {
        std::error_code ec;
        auto status = ctx.entry.status(ec);
        if (ec) return false;

        if (type == 'f') return fs::is_regular_file(status);
        if (type == 'd') return fs::is_directory(status);
        if (type == 'l') return fs::is_symlink(status);
        return false;
    }
};

// -size [+|-]N[c|k|M|G] Selector
class SizeExpr : public Expression {
    char op = '=';
    uint64_t target_size = 0;
public:
    SizeExpr(const std::string& spec) {
        std::string s = spec;
        if (s.empty()) return;
        if (s[0] == '+' || s[0] == '-') {
            op = s[0];
            s = s.substr(1);
        }
        uint64_t multiplier = 512; // Default 512-byte blocks
        char last = s.back();
        if (last == 'c') { multiplier = 1; s.pop_back(); }
        else if (last == 'k') { multiplier = 1024; s.pop_back(); }
        else if (last == 'M') { multiplier = 1024 * 1024; s.pop_back(); }
        else if (last == 'G') { multiplier = 1024ULL * 1024 * 1024; s.pop_back(); }

        target_size = std::stoull(s) * multiplier;
    }

    bool evaluate(const FindContext& ctx) override {
        std::error_code ec;
        if (!ctx.entry.is_regular_file(ec)) return false;
        uint64_t fsize = ctx.entry.file_size(ec);
        if (ec) return false;

        if (op == '+') return fsize > target_size;
        if (op == '-') return fsize < target_size;
        return fsize == target_size;
    }
};

// -mtime / -mmin Selector
class MTimeExpr : public Expression {
    char op = '=';
    int64_t target_units = 0;
    bool is_minutes = false;
public:
    MTimeExpr(const std::string& spec, bool minutes) : is_minutes(minutes) {
        std::string s = spec;
        if (!s.empty() && (s[0] == '+' || s[0] == '-')) {
            op = s[0];
            s = s.substr(1);
        }
        target_units = std::stoll(s);
    }

    bool evaluate(const FindContext& ctx) override {
        std::error_code ec;
        auto ftime = ctx.entry.last_write_time(ec);
        if (ec) return false;

        auto s_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto now = std::chrono::system_clock::now();

        int64_t diff = 0;
        if (is_minutes) {
            diff = std::chrono::duration_cast<std::chrono::minutes>(now - s_time).count();
        } else {
            diff = std::chrono::duration_cast<std::chrono::hours>(now - s_time).count() / 24;
        }

        if (op == '+') return diff > target_units;
        if (op == '-') return diff < target_units;
        return diff == target_units;
    }
};

// -empty Selector
class EmptyExpr : public Expression {
public:
    bool evaluate(const FindContext& ctx) override {
        std::error_code ec;
        if (ctx.entry.is_regular_file(ec)) {
            return ctx.entry.file_size(ec) == 0;
        } else if (ctx.entry.is_directory(ec)) {
            fs::directory_iterator it(ctx.entry.path(), ec);
            return !ec && (it == fs::directory_iterator());
        }
        return false;
    }
};

// -print / -print0 Action
class PrintExpr : public Expression {
    char delim;
public:
    PrintExpr(char d = '\n') : delim(d) {}

    bool evaluate(const FindContext& ctx) override {
        fs::path p = ctx.entry.path();
        p.make_preferred();
        std::cout << pathToUtf8(p) << delim;
        return true;
    }
    bool is_action() const override { return true; }
};

// -delete Action
class DeleteExpr : public Expression {
public:
    bool evaluate(const FindContext& ctx) override {
        std::error_code ec;
        fs::remove_all(ctx.entry.path(), ec);
        return !ec;
    }
    bool is_action() const override { return true; }
};

// -exec Action
class ExecExpr : public Expression {
    std::vector<std::string> command_template;
public:
    ExecExpr(const std::vector<std::string>& cmd) : command_template(cmd) {}

    bool evaluate(const FindContext& ctx) override {
        fs::path p = ctx.entry.path();
        p.make_preferred();
        std::string path_str = pathToUtf8(p);

        std::string full_cmd;
        for (size_t i = 0; i < command_template.size(); ++i) {
            std::string arg = command_template[i];
            size_t pos = 0;
            while ((pos = arg.find("{}", pos)) != std::string::npos) {
                arg.replace(pos, 2, path_str);
                pos += path_str.length();
            }
            if (i > 0) full_cmd += " ";
            full_cmd += arg;
        }

        int ret = std::system(full_cmd.c_str());
        return ret == 0;
    }
    bool is_action() const override { return true; }
};

// Recursive Descent Expression Parser
class Parser {
    std::vector<std::string> tokens;
    size_t pos = 0;
    int max_depth = INT_MAX;
    int min_depth = 0;
    bool action_present = false;

public:
    Parser(const std::vector<std::string>& t) : tokens(t) {}

    std::shared_ptr<Expression> parse() {
        if (tokens.empty()) return nullptr;
        auto expr = parse_or();
        if (pos < tokens.size()) {
            throw std::runtime_error("locate: unexpected argument '" + tokens[pos] + "'");
        }
        return expr;
    }

    int get_max_depth() const { return max_depth; }
    int get_min_depth() const { return min_depth; }
    bool has_action() const { return action_present; }

private:
    std::shared_ptr<Expression> parse_or() {
        auto left = parse_and();
        while (pos < tokens.size()) {
            if (tokens[pos] == "-o" || tokens[pos] == "-or") {
                pos++;
                auto right = parse_and();
                left = std::make_shared<OrExpr>(left, right);
            } else {
                break;
            }
        }
        return left;
    }

    std::shared_ptr<Expression> parse_and() {
        auto left = parse_not();
        while (pos < tokens.size()) {
            if (tokens[pos] == "-a" || tokens[pos] == "-and") {
                pos++;
                auto right = parse_not();
                left = std::make_shared<AndExpr>(left, right);
            } else if (tokens[pos] != "-o" && tokens[pos] != "-or" && tokens[pos] != ")") {
                // Implicit juxtaposition AND
                auto right = parse_not();
                left = std::make_shared<AndExpr>(left, right);
            } else {
                break;
            }
        }
        return left;
    }

    std::shared_ptr<Expression> parse_not() {
        if (pos < tokens.size() && (tokens[pos] == "!" || tokens[pos] == "-not")) {
            pos++;
            auto child = parse_not();
            return std::make_shared<NotExpr>(child);
        }
        return parse_primary();
    }

    std::shared_ptr<Expression> parse_primary() {
        if (pos >= tokens.size()) return nullptr;

        std::string tok = tokens[pos++];
        if (tok == "(") {
            auto expr = parse_or();
            if (pos < tokens.size() && tokens[pos] == ")") {
                pos++;
            } else {
                throw std::runtime_error("locate: missing closing parenthesis ')'");
            }
            return expr;
        }

        if (tok == "-name" || tok == "-iname") {
            if (pos >= tokens.size()) throw std::runtime_error("locate: missing argument to " + tok);
            return std::make_shared<NameExpr>(tokens[pos++], tok == "-iname");
        }

        if (tok == "-path" || tok == "-ipath") {
            if (pos >= tokens.size()) throw std::runtime_error("locate: missing argument to " + tok);
            return std::make_shared<PathExpr>(tokens[pos++], tok == "-ipath");
        }

        if (tok == "-type") {
            if (pos >= tokens.size()) throw std::runtime_error("locate: missing argument to -type");
            return std::make_shared<TypeExpr>(tokens[pos++][0]);
        }

        if (tok == "-size") {
            if (pos >= tokens.size()) throw std::runtime_error("locate: missing argument to -size");
            return std::make_shared<SizeExpr>(tokens[pos++]);
        }

        if (tok == "-mtime" || tok == "-mmin") {
            if (pos >= tokens.size()) throw std::runtime_error("locate: missing argument to " + tok);
            return std::make_shared<MTimeExpr>(tokens[pos++], tok == "-mmin");
        }

        if (tok == "-empty") {
            return std::make_shared<EmptyExpr>();
        }

        if (tok == "-maxdepth") {
            if (pos >= tokens.size()) throw std::runtime_error("locate: missing argument to -maxdepth");
            max_depth = std::stoi(tokens[pos++]);
            return std::make_shared<TrueExpr>();
        }

        if (tok == "-mindepth") {
            if (pos >= tokens.size()) throw std::runtime_error("locate: missing argument to -mindepth");
            min_depth = std::stoi(tokens[pos++]);
            return std::make_shared<TrueExpr>();
        }

        if (tok == "-print") {
            action_present = true;
            return std::make_shared<PrintExpr>('\n');
        }

        if (tok == "-print0") {
            action_present = true;
            return std::make_shared<PrintExpr>('\0');
        }

        if (tok == "-delete") {
            action_present = true;
            return std::make_shared<DeleteExpr>();
        }

        if (tok == "-exec") {
            action_present = true;
            std::vector<std::string> cmd;
            while (pos < tokens.size() && tokens[pos] != ";") {
                cmd.push_back(tokens[pos++]);
            }
            if (pos < tokens.size() && tokens[pos] == ";") {
                pos++;
            } else {
                throw std::runtime_error("locate: missing terminating ';' for -exec");
            }
            return std::make_shared<ExecExpr>(cmd);
        }

        throw std::runtime_error("locate: unknown expression or option '" + tok + "'");
    }
};

int main(int argc, char* argv[]) {
    bool follow_symlinks = false;
    std::vector<std::string> start_paths;
    std::vector<std::string> expr_tokens;

    int i = 1;
    bool stop_options = false;

    while (i < argc) {
        std::string arg = argv[i];

        if (stop_options) {
            break;
        }

        if (arg == "--") {
            stop_options = true;
            ++i;
            break;
        }

        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        }

        if (arg == "--version") {
            PrintVersion();
            return 0;
        }

        if (arg == "-L") {
            follow_symlinks = true;
            i++;
        } else if (arg == "-P") {
            follow_symlinks = false;
            i++;
        } else {
            break;
        }
    }

    // Parse starting paths
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "!" || arg == "(" || arg == ")" || (!arg.empty() && arg[0] == '-')) {
            break; // Start of expression
        }
        start_paths.push_back(arg);
        i++;
    }

    // Default path to current directory '.' if none specified
    if (start_paths.empty()) {
        start_paths.push_back(".");
    }

    // Remaining tokens are expression components
    while (i < argc) {
        expr_tokens.push_back(CanonicalizeOption(argv[i++]));
    }

    try {
        Parser parser(expr_tokens);
        auto root_expr = parser.parse();

        // If expression is empty or no action (-print, -delete, etc.) specified, default to -print
        if (!root_expr) {
            root_expr = std::make_shared<PrintExpr>('\n');
        } else if (!parser.has_action()) {
            root_expr = std::make_shared<AndExpr>(root_expr, std::make_shared<PrintExpr>('\n'));
        }

        int max_depth = parser.get_max_depth();
        int min_depth = parser.get_min_depth();

        // Iterate through each starting path
        for (const auto& path_str : start_paths) {
            fs::path start_path(path_str);
            std::error_code ec;

            if (!fs::exists(start_path, ec)) {
                std::cerr << "locate: " << path_str << ": No such file or directory\n";
                continue;
            }

            // Evaluate starting directory/file itself at depth 0
            if (0 >= min_depth && 0 <= max_depth) {
                fs::directory_entry root_entry(start_path, ec);
                if (!ec) {
                    FindContext ctx{ root_entry, 0, start_path };
                    root_expr->evaluate(ctx);
                }
            }

            if (!fs::is_directory(start_path, ec)) continue;

            fs::directory_options options = fs::directory_options::skip_permission_denied;
            if (follow_symlinks) {
                options |= fs::directory_options::follow_directory_symlink;
            }

            fs::recursive_directory_iterator it(start_path, options, ec), end;
            if (ec) {
                std::cerr << "locate: " << path_str << ": " << ec.message() << "\n";
                continue;
            }

            while (it != end) {
                const auto& entry = *it;
                int depth = it.depth() + 1; // 1-based child depth

                if (depth > max_depth) {
                    it.pop();
                    continue;
                }

                if (depth >= min_depth) {
                    FindContext ctx{ entry, depth, start_path };
                    root_expr->evaluate(ctx);
                }

                it.increment(ec);
                if (ec) {
                    std::cerr << "locate: " << ec.message() << "\n";
                    ec.clear();
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    return 0;
}
