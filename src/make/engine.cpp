#include "engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

// ============================================================================
// MakefileParser
// ============================================================================

MakefileParser::MakefileParser(const std::unordered_map<std::string, std::string>& cli_overrides) {
    // Environment Default Precedence
    macros["MAKE"] = "make";
    macros["OS"]   = "Windows_NT";

    if (fs::exists("C:\\Program Files\\Microsoft Visual Studio")) {
        macros["CC"]  = "cl.exe";
        macros["CXX"] = "cl.exe";
        macros["RM"]  = "del /f /q";
    } else {
        macros["CC"]  = "gcc";
        macros["CXX"] = "g++";
        macros["RM"]  = "rm -f";
    }

    // Apply CLI Overrides BEFORE parsing
    for (const auto& [k, v] : cli_overrides) {
        macros[k] = v;
    }
}

std::string MakefileParser::GetEnv(const char* var) {
    char* val = nullptr;
    size_t len = 0;
    if (_dupenv_s(&val, &len, var) == 0 && val != nullptr) {
        std::string res(val);
        free(val);
        return res;
    }
    return "";
}

std::string MakefileParser::expand_macros(const std::string& text, const std::unordered_map<std::string, std::string>& local_vars) const {
    std::string result = text;
    bool expanded = true;
    int depth = 0;

    while (expanded && depth < 20) {
        expanded = false;
        depth++;
        size_t pos = 0;
        while ((pos = result.find('$', pos)) != std::string::npos) {
            if (pos + 1 >= result.size()) break;

            if (result[pos + 1] == '$') { // Escaped $$
                result.erase(pos, 1);
                pos++;
                continue;
            }

            std::string var_name;
            size_t var_end = pos + 1;

            if (result[pos + 1] == '(' || result[pos + 1] == '{') {
                char close_char = (result[pos + 1] == '(') ? ')' : '}';
                size_t end_bracket = result.find(close_char, pos + 2);
                if (end_bracket != std::string::npos) {
                    var_name = result.substr(pos + 2, end_bracket - (pos + 2));
                    var_end = end_bracket;
                }
            } else {
                var_name = result.substr(pos + 1, 1);
            }

            if (!var_name.empty()) {
                std::string val;
                auto it_loc = local_vars.find(var_name);
                if (it_loc != local_vars.end()) {
                    val = it_loc->second;
                } else {
                    auto it_mac = macros.find(var_name);
                    if (it_mac != macros.end()) {
                        val = it_mac->second;
                    } else {
                        val = GetEnv(var_name.c_str());
                    }
                }

                result.replace(pos, var_end - pos + 1, val);
                expanded = true;
                pos += val.length();
            } else {
                pos++;
            }
        }
    }
    return result;
}

bool MakefileParser::parse(const fs::path& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    std::string current_target;

    while (std::getline(file, line)) {
        // Handle line continuation (\ at end)
        while (!line.empty() && line.back() == '\\') {
            line.pop_back();
            std::string next_line;
            if (std::getline(file, next_line)) {
                line += " " + trim(next_line);
            }
        }

        // Recipe lines start with TAB
        if (!line.empty() && line[0] == '\t') {
            if (!current_target.empty()) {
                rules[current_target].recipes.push_back(line.substr(1));
            }
            continue;
        }

        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        // Check for Macro Assignment (=, :=, +=, ?=)
        size_t eq_pos = trimmed.find('=');
        if (eq_pos != std::string::npos && (trimmed.find(':') == std::string::npos || eq_pos < trimmed.find(':'))) {
            std::string var_name = trim(trimmed.substr(0, eq_pos));
            std::string op = "=";
            if (!var_name.empty() && (var_name.back() == ':' || var_name.back() == '+' || var_name.back() == '?')) {
                op = var_name.back() + op;
                var_name.pop_back();
                var_name = trim(var_name);
            }

            std::string raw_val = trim(trimmed.substr(eq_pos + 1));
            if (op == ":=") {
                macros[var_name] = expand_macros(raw_val);
            } else if (op == "+=") {
                macros[var_name] += " " + expand_macros(raw_val);
            } else if (op == "?=") {
                if (macros.find(var_name) == macros.end()) macros[var_name] = expand_macros(raw_val);
            } else {
                macros[var_name] = raw_val;
            }
            current_target = "";
            continue;
        }

        // Check for Rule Declaration (Target : Prerequisites)
        size_t colon_pos = trimmed.find(':');
        if (colon_pos != std::string::npos) {
            std::string targets_part = trim(trimmed.substr(0, colon_pos));
            std::string prereqs_part = trim(trimmed.substr(colon_pos + 1));

            std::stringstream ss_t(targets_part);
            std::string tgt;
            std::vector<std::string> parsed_targets;
            while (ss_t >> tgt) parsed_targets.push_back(tgt);

            std::stringstream ss_p(prereqs_part);
            std::string prq;
            std::vector<std::string> prereqs;
            while (ss_p >> prq) prereqs.push_back(prq);

            for (const auto& t : parsed_targets) {
                if (t == ".PHONY") {
                    for (const auto& p : prereqs) phony_targets.insert(p);
                } else {
                    // Check for Suffix Rule (e.g., .cpp.obj)
                    if (t.size() > 2 && t[0] == '.' && t.find('.', 1) != std::string::npos) {
                        size_t second_dot = t.find('.', 1);
                        std::string src_ext = t.substr(0, second_dot);
                        std::string tgt_ext = t.substr(second_dot);
                        suffix_rules.push_back({src_ext, tgt_ext});
                    }

                    if (default_target.empty() && t[0] != '.') {
                        default_target = t;
                    }
                    Rule rule;
                    rule.target = t;
                    rule.prerequisites = prereqs;
                    rules[t] = rule;
                    current_target = t;
                }
            }
        }
    }

    for (const auto& p : phony_targets) {
        if (rules.find(p) != rules.end()) {
            rules[p].is_phony = true;
        }
    }
    return true;
}

bool MakefileParser::resolve_implicit_rule(const std::string& target, Rule& synthesized_rule) {
    if (rules.find(target) != rules.end()) {
        synthesized_rule = rules[target];
        return true;
    }

    for (const auto& [src_ext, tgt_ext] : suffix_rules) {
        if (target.size() > tgt_ext.size() && 
            target.compare(target.size() - tgt_ext.size(), tgt_ext.size(), tgt_ext) == 0) {
            
            std::string stem = target.substr(0, target.size() - tgt_ext.size());
            std::string src_file = stem + src_ext;

            if (fs::exists(src_file)) {
                std::string suffix_rule_name = src_ext + tgt_ext;
                if (rules.find(suffix_rule_name) != rules.end()) {
                    synthesized_rule = rules[suffix_rule_name];
                    synthesized_rule.target = target;
                    synthesized_rule.prerequisites.push_back(src_file);
                    return true;
                }
            }
        }
    }
    return false;
}

// ============================================================================
// ParallelBuildEngine
// ============================================================================

ParallelBuildEngine::ParallelBuildEngine(const Config& cfg, MakefileParser& p)
    : config(cfg), parser(p), pool(cfg.jobs) {}

bool ParallelBuildEngine::execute(const std::vector<std::string>& goals) {
    for (const auto& goal : goals) {
        if (!resolve_dag_nodes(goal)) {
            return false;
        }
    }

    size_t ready_count = 0;
    for (auto& [name, node] : dag) {
        if (node->pending_prereqs == 0) {
            ready_count++;
            active_tasks++;
            pool.enqueue([this, node] { process_node(node); });
        }
    }

    if (ready_count == 0 && !dag.empty()) {
        std::cerr << Color::RED << "make: *** Circular dependency detected in graph. Stop.\n" << Color::RESET;
        return false;
    }

    std::unique_lock<std::mutex> lock(engine_mutex);
    build_cv.wait(lock, [this] { return active_tasks == 0; });

    return !global_failure;
}

void ParallelBuildEngine::export_compilation_database() {
    std::ofstream out("compile_commands.json");
    out << "[\n";
    bool first = true;
    fs::path cwd = fs::current_path();

    for (const auto& [name, rule] : parser.rules) {
        for (const auto& recipe : rule.recipes) {
            std::string cmd = parser.expand_macros(recipe);
            if (cmd.find("gcc") != std::string::npos || cmd.find("g++") != std::string::npos ||
                cmd.find("cl") != std::string::npos || cmd.find("clang") != std::string::npos) {
                
                if (!first) out << ",\n";
                first = false;

                out << "  {\n"
                    << "    \"directory\": \"" << cwd.string() << "\",\n"
                    << "    \"command\": \"" << cmd << "\",\n"
                    << "    \"file\": \"" << (rule.prerequisites.empty() ? "" : rule.prerequisites[0]) << "\"\n"
                    << "  }";
            }
        }
    }
    out << "\n]\n";
}

void ParallelBuildEngine::dump_json_telemetry() {
    std::cout << "[\n";
    for (size_t i = 0; i < json_events.size(); ++i) {
        std::cout << "  " << json_events[i] << (i + 1 < json_events.size() ? "," : "") << "\n";
    }
    std::cout << "]\n";
}

bool ParallelBuildEngine::needs_rebuild(const Rule& rule, const std::string& target_name) {
    if (config.always_build || rule.is_phony) return true;
    if (!fs::exists(target_name)) return true;

    if (config.hash_check) {
        if (hash_cache.is_changed(target_name)) return true;
    }

    auto target_time = fs::last_write_time(target_name);
    for (const auto& prq : rule.prerequisites) {
        if (!fs::exists(prq)) return true;
        if (fs::last_write_time(prq) > target_time) return true;
    }
    return false;
}

bool ParallelBuildEngine::resolve_dag_nodes(const std::string& target_name) {
    if (dag.find(target_name) != dag.end()) return true;

    auto node = std::make_shared<DAGNode>();
    node->name = target_name;

    Rule rule;
    if (!parser.resolve_implicit_rule(target_name, rule)) {
        if (fs::exists(target_name)) {
            node->needs_build = false;
            dag[target_name] = node;
            return true;
        }
        std::cerr << Color::RED << "make: *** No rule to make target '" << target_name << "'. Stop.\n" << Color::RESET;
        return false;
    }

    node->rule = rule;
    node->needs_build = needs_rebuild(rule, target_name);
    dag[target_name] = node;

    for (const auto& prq : rule.prerequisites) {
        if (!resolve_dag_nodes(prq)) return false;

        auto prq_node = dag[prq];
        prq_node->dependents.push_back(target_name);
        node->pending_prereqs++;
    }

    return true;
}

void ParallelBuildEngine::process_node(std::shared_ptr<DAGNode> node) {
    bool success = true;

    if (global_failure && !config.keep_going) {
        finish_task(node, false);
        return;
    }

    if (node->needs_build && !node->rule.recipes.empty()) {
        success = run_recipes(node->rule);
    }

    if (!success) {
        node->failed = true;
        global_failure = true;
    }

    // Notify dependent targets
    for (const auto& dep_name : node->dependents) {
        auto dep_node = dag[dep_name];
        if (--dep_node->pending_prereqs == 0) {
            active_tasks++;
            pool.enqueue([this, dep_node] { process_node(dep_node); });
        }
    }

    finish_task(node, success);
}

void ParallelBuildEngine::finish_task(std::shared_ptr<DAGNode> node, bool success) {
    if (--active_tasks == 0) {
        std::unique_lock<std::mutex> lock(engine_mutex);
        build_cv.notify_all();
    }
}

bool ParallelBuildEngine::run_recipes(const Rule& rule) {
    std::unordered_map<std::string, std::string> auto_vars;
    auto_vars["@"] = rule.target;
    if (!rule.prerequisites.empty()) {
        auto_vars["<"] = rule.prerequisites[0];
        std::string all_prqs;
        for (const auto& p : rule.prerequisites) all_prqs += p + " ";
        if (!all_prqs.empty()) all_prqs.pop_back();
        auto_vars["^"] = all_prqs;
    }

    size_t task_id = ++completed_counter;

    for (const auto& raw_recipe : rule.recipes) {
        std::string cmd = parser.expand_macros(raw_recipe, auto_vars);
        cmd = trim(cmd);
        if (cmd.empty()) continue;

        bool echo = true;
        bool ignore_error = false;

        while (!cmd.empty() && (cmd[0] == '@' || cmd[0] == '-')) {
            if (cmd[0] == '@') echo = false;
            if (cmd[0] == '-') ignore_error = true;
            cmd = cmd.substr(1);
        }

        if (!config.silent && echo) {
            std::lock_guard<std::mutex> lock(engine_mutex);
            std::cout << Color::CYAN << "[" << task_id << "] " << Color::RESET << cmd << "\n";
        }

        if (config.dry_run) continue;

        auto start_time = std::chrono::high_resolution_clock::now();

        std::string exec_cmd = "cmd.exe /c " + cmd;
        int exit_code = ::system(exec_cmd.c_str());

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_time
        ).count();

        if (config.json_output) {
            std::stringstream json_event;
            json_event << "{\"target\":\"" << rule.target << "\",\"command\":\"" << cmd 
                       << "\",\"duration_ms\":" << duration << ",\"exit_code\":" << exit_code << "}";
            std::lock_guard<std::mutex> lock(engine_mutex);
            json_events.push_back(json_event.str());
        }

        if (exit_code != 0 && !ignore_error) {
            std::lock_guard<std::mutex> lock(engine_mutex);
            std::cerr << Color::RED << "make: *** [" << rule.target << "] Error " << exit_code << Color::RESET << "\n";
            return false;
        }
    }
    return true;
}
