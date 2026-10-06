#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "make.hpp"

class MakefileParser {
public:
    std::unordered_map<std::string, std::string> macros;
    std::unordered_map<std::string, Rule> rules;
    std::vector<std::pair<std::string, std::string>> suffix_rules;
    std::string default_target;
    std::unordered_set<std::string> phony_targets;

    explicit MakefileParser(const std::unordered_map<std::string, std::string>& cli_overrides);
    static std::string GetEnv(const char* var);
    std::string expand_macros(const std::string& text, const std::unordered_map<std::string, std::string>& local_vars = {}) const;
    bool parse(const fs::path& filepath);
    bool resolve_implicit_rule(const std::string& target, Rule& synthesized_rule);
};

class ParallelBuildEngine {
    Config config;
    MakefileParser parser;
    HashCache hash_cache;
    ThreadPool pool;

    std::unordered_map<std::string, std::shared_ptr<DAGNode>> dag;
    std::mutex engine_mutex;
    std::condition_variable build_cv;
    std::atomic<size_t> active_tasks{0};
    std::atomic<bool> global_failure{false};
    std::atomic<size_t> completed_counter{0};
    std::vector<std::string> json_events;

    bool needs_rebuild(const Rule& rule, const std::string& target_name);
    bool resolve_dag_nodes(const std::string& target_name);
    void process_node(std::shared_ptr<DAGNode> node);
    void finish_task(std::shared_ptr<DAGNode> node, bool success);
    bool run_recipes(const Rule& rule);

public:
    ParallelBuildEngine(const Config& cfg, MakefileParser& p);
    bool execute(const std::vector<std::string>& goals);
    void export_compilation_database();
    void dump_json_telemetry();
};

#endif // ENGINE_HPP
