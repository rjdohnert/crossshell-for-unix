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
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <set>
#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <windows.h>

namespace fs = std::filesystem;

constexpr char PROG_NAME[] = "make";
constexpr char VERSION[]   = "2026.2.0";

// --- Terminal Colors (ANSI Virtual Terminal) ---
namespace Color {
    const char* RESET   = "\033[0m";
    const char* RED     = "\033[91m";
    const char* GREEN   = "\033[92m";
    const char* YELLOW  = "\033[93m";
    const char* BLUE    = "\033[94m";
    const char* CYAN    = "\033[96m";
    const char* BOLD    = "\033[1m";
    bool enabled = true;
}

// --- FNV-1a 64-bit Fast Content Hasher ---
uint64_t hash_file_content(const fs::path& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return 0;

    uint64_t hash = 14695981039346656037ULL;
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::streamsize bytes = file.gcount();
        for (std::streamsize i = 0; i < bytes; ++i) {
            hash ^= static_cast<uint8_t>(buffer[i]);
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

// Trim whitespace helpers
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// --- ThreadPool for True Parallel Builds (-j N) ---
class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};

public:
    ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->cv.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(task);
        }
        cv.notify_one();
    }

    ~ThreadPool() {
        stop = true;
        cv.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};

// --- Configuration Options ---
struct Config {
    std::string makefile_path;
    std::vector<std::string> target_goals;
    std::unordered_map<std::string, std::string> cli_macros;
    unsigned int jobs = std::thread::hardware_concurrency();
    bool dry_run = false;
    bool silent = false;
    bool keep_going = false;
    bool always_build = false;
    bool hash_check = false;
    bool export_compile_commands = false;
    bool json_output = false;
    bool show_help = false;
    bool show_version = false;
    fs::path change_dir;
};

// --- Target Rule Representation ---
struct Rule {
    std::string target;
    std::vector<std::string> prerequisites;
    std::vector<std::string> recipes;
    bool is_phony = false;
};

// --- Hash Persistence Cache (.make.hash) ---
class HashCache {
    std::unordered_map<std::string, uint64_t> cache;
    fs::path cache_file = ".make.hash";
    std::mutex cache_mutex;

public:
    HashCache() {
        if (fs::exists(cache_file)) {
            std::ifstream in(cache_file);
            std::string file;
            uint64_t hash;
            while (in >> file >> hash) {
                cache[file] = hash;
            }
        }
    }

    ~HashCache() {
        std::ofstream out(cache_file);
        for (const auto& [file, hash] : cache) {
            out << file << " " << hash << "\n";
        }
    }

    bool is_changed(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (!fs::exists(filepath)) return true;
        uint64_t current_hash = hash_file_content(filepath);
        auto it = cache.find(filepath);
        if (it == cache.end() || it->second != current_hash) {
            cache[filepath] = current_hash;
            return true;
        }
        return false;
    }
};

// --- Makefile Parser & Macro Engine ---
class MakefileParser {
public:
    std::unordered_map<std::string, std::string> macros;
    std::unordered_map<std::string, Rule> rules;
    std::vector<std::pair<std::string, std::string>> suffix_rules; // e.g., (".cpp", ".obj")
    std::string default_target;
    std::unordered_set<std::string> phony_targets;

    MakefileParser(const std::unordered_map<std::string, std::string>& cli_overrides) {
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

    static std::string GetEnv(const char* var) {
        char* val = nullptr;
        size_t len = 0;
        if (_dupenv_s(&val, &len, var) == 0 && val != nullptr) {
            std::string res(val);
            free(val);
            return res;
        }
        return "";
    }

    std::string expand_macros(const std::string& text, const std::unordered_map<std::string, std::string>& local_vars = {}) const {
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

    bool parse(const fs::path& filepath) {
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

    // Resolve Implicit Suffix Rules (.cpp.obj, .c.o)
    bool resolve_implicit_rule(const std::string& target, Rule& synthesized_rule) {
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
};

// --- DAG Target Node for Parallel Scheduler ---
struct DAGNode {
    std::string name;
    Rule rule;
    std::atomic<int> pending_prereqs{0};
    std::vector<std::string> dependents;
    bool needs_build = false;
    bool failed = false;
};

// --- Parallel Build Engine ---
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

public:
    ParallelBuildEngine(const Config& cfg, MakefileParser& p)
        : config(cfg), parser(p), pool(cfg.jobs) {}

    bool execute(const std::vector<std::string>& goals) {
        // 1. Build DAG graph for all goals
        for (const auto& goal : goals) {
            if (!resolve_dag_nodes(goal)) {
                return false;
            }
        }

        // 2. Queue initial targets (zero pending prerequisites)
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

        // 3. Wait for all parallel tasks to complete
        std::unique_lock<std::mutex> lock(engine_mutex);
        build_cv.wait(lock, [this] { return active_tasks == 0; });

        return !global_failure;
    }

    void export_compilation_database() {
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

    void dump_json_telemetry() {
        std::cout << "[\n";
        for (size_t i = 0; i < json_events.size(); ++i) {
            std::cout << "  " << json_events[i] << (i + 1 < json_events.size() ? "," : "") << "\n";
        }
        std::cout << "]\n";
    }

private:
    bool needs_rebuild(const Rule& rule, const std::string& target_name) {
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

    bool resolve_dag_nodes(const std::string& target_name) {
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

    void process_node(std::shared_ptr<DAGNode> node) {
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

    void finish_task(std::shared_ptr<DAGNode> node, bool success) {
        if (--active_tasks == 0) {
            std::unique_lock<std::mutex> lock(engine_mutex);
            build_cv.notify_all();
        }
    }

    bool run_recipes(const Rule& rule) {
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
};

// --- Command Line Interface ---
void print_version() {
    std::cout << PROG_NAME << " version " << VERSION << "\n"
              << "\n"
              << "Copyright (c) 2026, Roberto J Dohnert. Licensed under the BSD 3-Clause License.\n";
}

void print_help() {
    std::cout << R"(make(1)                  CrossShell for UNIX Reference Manual                 make(1)

    NAME
        make - maintain, update, and regenerate groups of programs

    SYNOPSIS
        make [OPTIONS] [TARGET...] [MACRO=VALUE...]

    DESCRIPTION
        Controls the generation of executables and other non-source files of a program
        from the program's source files. It allows specifying dependencies, rules,
        parallel build execution, and custom macro overrides.

    OPTIONS
        -f, --file <file>
            Read FILE as a makefile.

        -j, --jobs [<n>]
            Allow N parallel build jobs (default: number of CPU cores).

        -n, --dry-run
            Print recipes without executing them.

        -s, --silent, --quiet
            Do not echo recipes before execution.

        -k, --keep-going
            Continue execution as much as possible after encountering errors.

        -B, --always-build
            Unconditionally build all targets regardless of modification times.

        -C, --directory <dir>
            Change directory to DIR before reading the makefile.

        -H, --hash-check
            Use fast content hashing (FNV-1a) instead of file timestamps.

        --export-compile-commands
            Export compile_commands.json for Clangd and MSVC IntelliSense.

        --json
            Output structured build telemetry in JSON format.

        --no-color
            Disable ANSI terminal color output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        -v, -V, --version
            Display version information and exit.

    EXAMPLES
        make
            Build default target from Makefile or makefile.

        make -j8 -H
            Perform parallel build with 8 jobs using content hashing.

        make -f Makefile.win CXX=clang++ --export-compile-commands
            Use custom makefile, override CXX compiler, and generate compilation database.

        make -C src clean
            Change to src directory and run clean target.

    EXIT STATUS
        0
            Successful build.
        1
            Make syntax error, build recipe failure, or target not found.

    CrossShell for UNIX                                                    make(1)
)";
}

bool parse_cli(int argc, char* argv[], Config& cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            cfg.show_help = true;
            return true;
        }
        if (arg == "-v" || arg == "-V" || arg == "--version") {
            cfg.show_version = true;
            return true;
        }
        if (arg == "-n" || arg == "--dry-run") {
            cfg.dry_run = true;
        } else if (arg == "-s" || arg == "--silent" || arg == "--quiet") {
            cfg.silent = true;
        } else if (arg == "-k" || arg == "--keep-going") {
            cfg.keep_going = true;
        } else if (arg == "-B" || arg == "--always-build") {
            cfg.always_build = true;
        } else if (arg == "-H" || arg == "--hash-check") {
            cfg.hash_check = true;
        } else if (arg == "--export-compile-commands") {
            cfg.export_compile_commands = true;
        } else if (arg == "--json") {
            cfg.json_output = true;
        } else if (arg == "--no-color") {
            Color::enabled = false;
        } else if (arg == "-f" || arg == "--file") {
            if (i + 1 < argc) cfg.makefile_path = argv[++i];
        } else if (arg == "-C" || arg == "--directory") {
            if (i + 1 < argc) cfg.change_dir = argv[++i];
        } else if (arg == "-j" || arg == "--jobs") {
            if (i + 1 < argc && std::isdigit(argv[i + 1][0])) {
                cfg.jobs = std::strtoul(argv[++i], nullptr, 10);
            }
        } else if (arg.find('=') != std::string::npos) {
            size_t eq = arg.find('=');
            cfg.cli_macros[arg.substr(0, eq)] = arg.substr(eq + 1);
        } else {
            cfg.target_goals.push_back(arg);
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    // Enable UTF-8 Codepage and Virtual Terminal Processing on Windows Console
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    Config cfg;
    if (!parse_cli(argc, argv, cfg)) return 1;

    if (cfg.show_help) {
        print_help();
        return 0;
    }

    if (cfg.show_version) {
        print_version();
        return 0;
    }

    if (!Color::enabled) {
        Color::RESET = Color::RED = Color::GREEN = Color::YELLOW = Color::BLUE = Color::CYAN = Color::BOLD = "";
    }

    if (!cfg.change_dir.empty()) {
        std::error_code ec;
        fs::current_path(cfg.change_dir, ec);
        if (ec) {
            std::cerr << Color::RED << "make: *** Unable to change directory to " << cfg.change_dir.string() << Color::RESET << "\n";
            return 1;
        }
    }

    if (cfg.makefile_path.empty()) {
        if (fs::exists("Makefile")) cfg.makefile_path = "Makefile";
        else if (fs::exists("makefile")) cfg.makefile_path = "makefile";
        else if (fs::exists("Makefile.win")) cfg.makefile_path = "Makefile.win";
        else {
            std::cerr << Color::RED << "make: *** No targets specified and no makefile found. Stop." << Color::RESET << "\n";
            return 1;
        }
    }

    // Apply CLI overrides during parser creation
    MakefileParser parser(cfg.cli_macros);
    if (!parser.parse(cfg.makefile_path)) {
        std::cerr << Color::RED << "make: *** Failed to parse makefile '" << cfg.makefile_path << "'. Stop." << Color::RESET << "\n";
        return 1;
    }

    ParallelBuildEngine engine(cfg, parser);

    if (cfg.export_compile_commands) {
        engine.export_compilation_database();
        std::cout << Color::GREEN << "make: Exported compile_commands.json\n" << Color::RESET;
    }

    std::vector<std::string> goals = cfg.target_goals;
    if (goals.empty()) {
        if (!parser.default_target.empty()) {
            goals.push_back(parser.default_target);
        } else {
            std::cerr << Color::RED << "make: *** No target found. Stop." << Color::RESET << "\n";
            return 1;
        }
    }

    auto build_start = std::chrono::high_resolution_clock::now();
    bool success = engine.execute(goals);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - build_start
    ).count();

    if (cfg.json_output) {
        engine.dump_json_telemetry();
    } else if (success && !cfg.silent) {
        std::cout << Color::GREEN << "make: Target(s) built successfully in " 
                  << elapsed << " ms." << Color::RESET << "\n";
    }

    return success ? 0 : 1;
}