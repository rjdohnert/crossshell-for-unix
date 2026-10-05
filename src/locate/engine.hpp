#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "locate.hpp"
#include "options.hpp"
#include <regex>
#include <climits>

std::regex wildcard_to_regex(const std::string& pattern, bool case_insensitive);

class AndExpr : public Expression {
    std::shared_ptr<Expression> left, right;
public:
    AndExpr(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r);
    bool evaluate(const FindContext& ctx) override;
};

class OrExpr : public Expression {
    std::shared_ptr<Expression> left, right;
public:
    OrExpr(std::shared_ptr<Expression> l, std::shared_ptr<Expression> r);
    bool evaluate(const FindContext& ctx) override;
};

class NotExpr : public Expression {
    std::shared_ptr<Expression> child;
public:
    explicit NotExpr(std::shared_ptr<Expression> c);
    bool evaluate(const FindContext& ctx) override;
};

class TrueExpr : public Expression {
public:
    bool evaluate(const FindContext&) override;
};

class NameExpr : public Expression {
    std::regex rx;
public:
    NameExpr(const std::string& pattern, bool icase);
    bool evaluate(const FindContext& ctx) override;
};

class PathExpr : public Expression {
    std::regex rx;
public:
    PathExpr(const std::string& pattern, bool icase);
    bool evaluate(const FindContext& ctx) override;
};

class TypeExpr : public Expression {
    char type;
public:
    explicit TypeExpr(char t);
    bool evaluate(const FindContext& ctx) override;
};

class SizeExpr : public Expression {
    char op = '=';
    uint64_t target_size = 0;
public:
    explicit SizeExpr(const std::string& spec);
    bool evaluate(const FindContext& ctx) override;
};

class MTimeExpr : public Expression {
    char op = '=';
    int64_t target_units = 0;
    bool is_minutes = false;
public:
    MTimeExpr(const std::string& spec, bool minutes);
    bool evaluate(const FindContext& ctx) override;
};

class EmptyExpr : public Expression {
public:
    bool evaluate(const FindContext& ctx) override;
};

class PrintExpr : public Expression {
    char delim;
public:
    explicit PrintExpr(char d = '\n');
    bool evaluate(const FindContext& ctx) override;
    bool is_action() const override;
};

class DeleteExpr : public Expression {
public:
    bool evaluate(const FindContext& ctx) override;
    bool is_action() const override;
};

class ExecExpr : public Expression {
    std::vector<std::string> command_template;
public:
    explicit ExecExpr(const std::vector<std::string>& cmd);
    bool evaluate(const FindContext& ctx) override;
    bool is_action() const override;
};

class Parser {
    std::vector<std::string> tokens;
    size_t pos = 0;
    int max_depth = INT_MAX;
    int min_depth = 0;
    bool action_present = false;

    std::shared_ptr<Expression> parse_or();
    std::shared_ptr<Expression> parse_and();
    std::shared_ptr<Expression> parse_not();
    std::shared_ptr<Expression> parse_primary();

public:
    explicit Parser(const std::vector<std::string>& t);
    std::shared_ptr<Expression> parse();
    int get_max_depth() const;
    int get_min_depth() const;
    bool has_action() const;
};

class LocateEngine {
public:
    static int execute(const LocateOptions& opts);
};

#endif // ENGINE_HPP
