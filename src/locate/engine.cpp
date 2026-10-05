#include "engine.hpp"
#include <iostream>
#include <chrono>
#include <cstdlib>

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

// AndExpr
AndExpr::AndExpr(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r)
    : left(std::move(l)), right(std::move(r)) {}

bool AndExpr::evaluate(const FindContext& ctx) {
    if (!left || !left->evaluate(ctx)) return false;
    if (!right) return true;
    return right->evaluate(ctx);
}

// OrExpr
OrExpr::OrExpr(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r)
    : left(std::move(l)), right(std::move(r)) {}

bool OrExpr::evaluate(const FindContext& ctx) {
    if (left && left->evaluate(ctx)) return true;
    if (right && right->evaluate(ctx)) return true;
    return false;
}

// NotExpr
NotExpr::NotExpr(std::shared_ptr<Expression> c) : child(std::move(c)) {}

bool NotExpr::evaluate(const FindContext& ctx) {
    return child ? !child->evaluate(ctx) : false;
}

// TrueExpr
bool TrueExpr::evaluate(const FindContext&) {
    return true;
}

// NameExpr
NameExpr::NameExpr(const std::string& pattern, bool icase)
    : rx(wildcard_to_regex(pattern, icase)) {}

bool NameExpr::evaluate(const FindContext& ctx) {
    std::string filename = pathToUtf8(ctx.entry.path().filename());
    return std::regex_match(filename, rx);
}

// PathExpr
PathExpr::PathExpr(const std::string& pattern, bool icase)
    : rx(wildcard_to_regex(pattern, icase)) {}

bool PathExpr::evaluate(const FindContext& ctx) {
    fs::path p = ctx.entry.path();
    p.make_preferred();
    return std::regex_match(pathToUtf8(p), rx);
}

// TypeExpr
TypeExpr::TypeExpr(char t) : type(t) {}

bool TypeExpr::evaluate(const FindContext& ctx) {
    std::error_code ec;
    auto status = ctx.entry.status(ec);
    if (ec) return false;

    if (type == 'f') return fs::is_regular_file(status);
    if (type == 'd') return fs::is_directory(status);
    if (type == 'l') return fs::is_symlink(status);
    return false;
}

// SizeExpr
SizeExpr::SizeExpr(const std::string& spec) {
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

bool SizeExpr::evaluate(const FindContext& ctx) {
    std::error_code ec;
    if (!ctx.entry.is_regular_file(ec)) return false;
    uint64_t fsize = ctx.entry.file_size(ec);
    if (ec) return false;

    if (op == '+') return fsize > target_size;
    if (op == '-') return fsize < target_size;
    return fsize == target_size;
}

// MTimeExpr
MTimeExpr::MTimeExpr(const std::string& spec, bool minutes) : is_minutes(minutes) {
    std::string s = spec;
    if (!s.empty() && (s[0] == '+' || s[0] == '-')) {
        op = s[0];
        s = s.substr(1);
    }
    target_units = std::stoll(s);
}

bool MTimeExpr::evaluate(const FindContext& ctx) {
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

// EmptyExpr
bool EmptyExpr::evaluate(const FindContext& ctx) {
    std::error_code ec;
    if (ctx.entry.is_regular_file(ec)) {
        return ctx.entry.file_size(ec) == 0;
    } else if (ctx.entry.is_directory(ec)) {
        fs::directory_iterator it(ctx.entry.path(), ec);
        return !ec && (it == fs::directory_iterator());
    }
    return false;
}

// PrintExpr
PrintExpr::PrintExpr(char d) : delim(d) {}

bool PrintExpr::evaluate(const FindContext& ctx) {
    fs::path p = ctx.entry.path();
    p.make_preferred();
    std::cout << pathToUtf8(p) << delim;
    return true;
}

bool PrintExpr::is_action() const {
    return true;
}

// DeleteExpr
bool DeleteExpr::evaluate(const FindContext& ctx) {
    std::error_code ec;
    fs::remove_all(ctx.entry.path(), ec);
    return !ec;
}

bool DeleteExpr::is_action() const {
    return true;
}

// ExecExpr
ExecExpr::ExecExpr(const std::vector<std::string>& cmd) : command_template(cmd) {}

bool ExecExpr::evaluate(const FindContext& ctx) {
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

bool ExecExpr::is_action() const {
    return true;
}

// Parser
Parser::Parser(const std::vector<std::string>& t) : tokens(t) {}

std::shared_ptr<Expression> Parser::parse() {
    if (tokens.empty()) return nullptr;
    auto expr = parse_or();
    if (pos < tokens.size()) {
        throw std::runtime_error("locate: unexpected argument '" + tokens[pos] + "'");
    }
    return expr;
}

int Parser::get_max_depth() const { return max_depth; }
int Parser::get_min_depth() const { return min_depth; }
bool Parser::has_action() const { return action_present; }

std::shared_ptr<Expression> Parser::parse_or() {
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

std::shared_ptr<Expression> Parser::parse_and() {
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

std::shared_ptr<Expression> Parser::parse_not() {
    if (pos < tokens.size() && (tokens[pos] == "!" || tokens[pos] == "-not")) {
        pos++;
        auto child = parse_not();
        return std::make_shared<NotExpr>(child);
    }
    return parse_primary();
}

std::shared_ptr<Expression> Parser::parse_primary() {
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

int LocateEngine::execute(const LocateOptions& opts) {
    try {
        Parser parser(opts.expr_tokens);
        auto root_expr = parser.parse();

        // If expression is empty or no action specified, default to -print
        if (!root_expr) {
            root_expr = std::make_shared<PrintExpr>('\n');
        } else if (!parser.has_action()) {
            root_expr = std::make_shared<AndExpr>(root_expr, std::make_shared<PrintExpr>('\n'));
        }

        int max_depth = parser.get_max_depth();
        int min_depth = parser.get_min_depth();

        // Iterate through each starting path
        for (const auto& path_str : opts.start_paths) {
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

            fs::directory_options dir_options = fs::directory_options::skip_permission_denied;
            if (opts.follow_symlinks) {
                dir_options |= fs::directory_options::follow_directory_symlink;
            }

            fs::recursive_directory_iterator it(start_path, dir_options, ec), end;
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
