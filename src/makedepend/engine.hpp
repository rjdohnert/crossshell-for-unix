#ifndef MAKEDEPEND_ENGINE_HPP
#define MAKEDEPEND_ENGINE_HPP

#include "makedepend.hpp"

std::string clean_line(const std::string& line, LineCleanerState& state);

class ExpressionEvaluator {
public:
    static long long evaluate(std::string expr, const std::map<std::string, MacroDef>& macros);
    static std::string expand_macros(std::string text, const std::map<std::string, MacroDef>& macros, int depth = 0);

private:
    std::string src;
    size_t pos = 0;

    explicit ExpressionEvaluator(std::string s);
    void skip_whitespace();
    char peek();
    char get();
    static std::string process_defined(const std::string& input, const std::map<std::string, MacroDef>& macros);
    long long parse_primary();
    long long parse_mul();
    long long parse_add();
    long long parse_shift();
    long long parse_relational();
    long long parse_equality();
    long long parse_bit_and();
    long long parse_bit_xor();
    long long parse_bit_or();
    long long parse_log_and();
    long long parse_log_or();
    long long parse_expr();
};

class DependencyParser {
public:
    std::vector<std::string> includeDirs;
    std::map<std::string, MacroDef> macros;
    std::set<std::string> pragmaOnceFiles;
    bool verbose = false;

    DependencyParser();
    void setup_msvc_defaults(const std::string& msvc_ver);
    void setup_gcc_defaults(const std::string& gcc_ver = "13");
    void setup_clang_defaults(const std::string& clang_ver = "17");
    void add_object_macro(const std::string& name, const std::string& body);
    void load_env_includes();
    void probe_compiler_includes(const std::string& compiler);
    void load_gcc_includes();
    void load_clang_includes();
    std::set<std::string, CaseInsensitivePathCompare> find_dependencies(const std::string& filepath);

private:
    std::string resolve_include(const std::string& header, const std::string& currentDir, bool isSystem);
    void parse_define(const std::string& line, std::map<std::string, MacroDef>& currentMacros);
    void parse_file(const std::string& filepath, 
                    std::set<std::string, CaseInsensitivePathCompare>& dependencies, 
                    std::set<std::string>& visitedFiles,
                    std::map<std::string, MacroDef>& currentMacros);
};

void update_makefile(const std::string& makefilePath, 
                     const std::string& delimiter, 
                     bool appendOnly, 
                     const std::map<std::string, std::set<std::string, CaseInsensitivePathCompare>>& allDeps);

#endif // MAKEDEPEND_ENGINE_HPP
