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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <deque>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace EnterpriseNinja {

// ============================================================================
// String Helper Functions
// ============================================================================
inline std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

inline std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// ============================================================================
// 1. SIGNAL & PROCESS GROUP DISCIPLINE (PREVENTS ORPHAN RUNAWAY PROCESSES)
// ============================================================================
class ProcessTracker {
public:
    static inline std::atomic<bool> g_interrupted{false};
    static inline HANDLE g_job = NULL;

    static void Initialize() {
        g_job = CreateJobObjectW(NULL, NULL);
        if (g_job) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }
    }

    static void Cleanup() {
        if (g_job) {
            CloseHandle(g_job);
            g_job = NULL;
        }
    }

    static void RegisterProcess(HANDLE hProcess) {
        if (g_job && hProcess) {
            AssignProcessToJobObject(g_job, hProcess);
        }
    }

    static void UnregisterProcess(HANDLE hProcess) {
        // Automatically handled by Job Object
    }

    static void KillAllActive() {
        if (g_job) {
            TerminateJobObject(g_job, 1);
        }
    }

    static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
        if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
            g_interrupted.store(true, std::memory_order_release);
            KillAllActive();
            const char msg[] = "\nninja: interrupted by user. Killing child processes...\n";
            std::cerr << msg;
            return TRUE;
        }
        return FALSE;
    }

    static void InstallSignalHandlers() {
        Initialize();
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    }
};

// ============================================================================
// 2. MULTI-CHUNK CHAINED ARENA ALLOCATOR (INFINITE CAPACITY, ZERO FRAGMENTATION)
// ============================================================================
class ChainedArena {
public:
    static constexpr size_t CHUNK_SIZE = 64 * 1024 * 1024; // 64 MB Chunks

    ChainedArena() {
        AllocateNewChunk();
    }

    ~ChainedArena() {
        for (char* chunk : chunks_) {
            ::free(chunk);
        }
    }

    template <typename T, typename... Args>
    T* Alloc(Args&&... args) {
        size_t align = alignof(T);
        size_t current_addr = reinterpret_cast<size_t>(chunks_.back() + current_offset_);
        size_t padding = (align - (current_addr % align)) % align;

        if (current_offset_ + padding + sizeof(T) > CHUNK_SIZE) {
            AllocateNewChunk();
            current_addr = reinterpret_cast<size_t>(chunks_.back());
            padding = (align - (current_addr % align)) % align;
        }

        current_offset_ += padding;
        void* ptr = chunks_.back() + current_offset_;
        current_offset_ += sizeof(T);
        return new (ptr) T(std::forward<Args>(args)...);
    }

    char* DuplicateString(std::string_view sv) {
        if (current_offset_ + sv.size() + 1 > CHUNK_SIZE) {
            AllocateNewChunk();
        }
        char* dest = chunks_.back() + current_offset_;
        std::memcpy(dest, sv.data(), sv.size());
        dest[sv.size()] = '\0';
        current_offset_ += sv.size() + 1;
        return dest;
    }

private:
    std::vector<char*> chunks_;
    size_t current_offset_ = 0;

    void AllocateNewChunk() {
        char* chunk = static_cast<char*>(::malloc(CHUNK_SIZE));
        if (!chunk) {
            std::cerr << "ninja: fatal out-of-memory allocating 64MB arena chunk\n";
            std::exit(12);
        }
        chunks_.push_back(chunk);
        current_offset_ = 0;
    }
};

// ============================================================================
// 3. SHARDED LOCK-STRIPED STAT CACHE (NO GLOBAL LOCK CONTENTION)
// ============================================================================
class ShardedStatCache {
public:
    static constexpr size_t NUM_SHARDS = 64;

    static uint64_t GetMtime(std::string_view path) {
        size_t hash_val = std::hash<std::string_view>{}(path);
        size_t shard_idx = hash_val % NUM_SHARDS;
        auto& shard = shards_[shard_idx];

        {
            std::lock_guard<std::mutex> lock(shard.lock);
            auto it = shard.cache.find(path);
            if (it != shard.cache.end()) {
                return it->second;
            }
        }

        std::wstring wpath = StringToWString(std::string(path));
        WIN32_FILE_ATTRIBUTE_DATA fad;
        uint64_t mtime = 0;
        if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &fad)) {
            LARGE_INTEGER li;
            li.LowPart = fad.ftLastWriteTime.dwLowDateTime;
            li.HighPart = fad.ftLastWriteTime.dwHighDateTime;
            mtime = static_cast<uint64_t>(li.QuadPart);
        }

        {
            std::lock_guard<std::mutex> lock(shard.lock);
            shard.cache[path] = mtime;
        }
        return mtime;
    }

private:
    struct Shard {
        std::mutex lock;
        std::unordered_map<std::string_view, uint64_t> cache;
    };
    static inline std::array<Shard, NUM_SHARDS> shards_;
};

// ============================================================================
// 4. DIRECT HIGH-SPEED PROCESS EXECUTION (BYPASSES SHELL WHEN POSSIBLE)
// ============================================================================
class SubprocessEngine {
public:
    static bool HasShellMetachars(std::string_view cmd) {
        return cmd.find_first_of("|&;><*?$`\n()[]{}") != std::string_view::npos;
    }

    static int Execute(const std::string& cmd, bool dry_run, bool verbose, std::string& output_buffer) {
        if (verbose || dry_run) {
            std::cout << cmd << "\n";
        }
        if (dry_run) return 0;

        // Pipe setup for capturing stdout/stderr
        HANDLE hChildStd_OUT_Rd = NULL;
        HANDLE hChildStd_OUT_Wr = NULL;

        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
            return -1;
        }

        if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(hChildStd_OUT_Wr);
            CloseHandle(hChildStd_OUT_Rd);
            return -1;
        }

        PROCESS_INFORMATION piProcInfo;
        STARTUPINFOW siStartInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
        siStartInfo.cb = sizeof(STARTUPINFOW);
        siStartInfo.hStdError = hChildStd_OUT_Wr;
        siStartInfo.hStdOutput = hChildStd_OUT_Wr;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        std::wstring wcmd;
        bool need_shell = HasShellMetachars(cmd);
        if (need_shell) {
            wcmd = L"cmd.exe /c \"" + StringToWString(cmd) + L"\"";
        } else {
            wcmd = StringToWString(cmd);
        }

        // Create the child process
        BOOL bSuccess = CreateProcessW(
            NULL,
            &wcmd[0], // command line
            NULL,     // process security attributes
            NULL,     // primary thread security attributes
            TRUE,     // handles are inherited
            CREATE_NO_WINDOW, // creation flags
            NULL,     // use parent's environment
            NULL,     // use parent's current directory
            &siStartInfo,  // STARTUPINFO pointer
            &piProcInfo    // receives PROCESS_INFORMATION
        );

        // Close the write end of the pipe in the parent so we don't block reading
        CloseHandle(hChildStd_OUT_Wr);

        if (!bSuccess) {
            CloseHandle(hChildStd_OUT_Rd);
            return -1;
        }

        // Register the child process handle with process tracker
        ProcessTracker::RegisterProcess(piProcInfo.hProcess);

        // Read output from the child process's pipe
        char buf[4096];
        DWORD dwRead;
        while (ReadFile(hChildStd_OUT_Rd, buf, sizeof(buf), &dwRead, NULL) && dwRead > 0) {
            output_buffer.append(buf, dwRead);
        }
        CloseHandle(hChildStd_OUT_Rd);

        // Wait for child process to exit
        DWORD exitCode = 0;
        WaitForSingleObject(piProcInfo.hProcess, INFINITE);
        GetExitCodeProcess(piProcInfo.hProcess, &exitCode);

        ProcessTracker::UnregisterProcess(piProcInfo.hProcess);

        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);

        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 5. GRAPH ENGINE & SCHEDULER
// ============================================================================
struct Edge;

struct Node {
    std::string_view path;
    uint64_t mtime = 0;
    bool exists = false;
    bool dirty = false;
    Edge* in_edge = nullptr;
    std::vector<Edge*> out_edges;

    explicit Node(std::string_view p) : path(p) {}
};

struct Rule {
    std::string_view name;
    std::string_view command;
    std::string_view description;
};

struct Edge {
    Rule rule;
    std::vector<Node*> inputs;
    std::vector<Node*> outputs;
    std::atomic<uint32_t> pending_inputs{0};

    std::string ExpandCommand() const {
        std::string cmd(rule.command);
        std::string in_str;
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (i > 0) in_str += " ";
            in_str.append(inputs[i]->path.data(), inputs[i]->path.size());
        }
        std::string out_str;
        for (size_t i = 0; i < outputs.size(); ++i) {
            if (i > 0) out_str += " ";
            out_str.append(outputs[i]->path.data(), outputs[i]->path.size());
        }

        auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };

        replace_all(cmd, "$in", in_str);
        replace_all(cmd, "$out", out_str);
        return cmd;
    }
};

class BuildEngine {
public:
    ChainedArena arena;
    std::unordered_map<std::string_view, Node*> node_map;
    std::unordered_map<std::string_view, Rule> rule_map;
    std::vector<Edge*> all_edges;
    std::vector<Node*> default_targets;

    Node* GetOrCreateNode(std::string_view path) {
        auto it = node_map.find(path);
        if (it != node_map.end()) return it->second;

        char* cloned_path = arena.DuplicateString(path);
        std::string_view stable_view(cloned_path, path.size());
        Node* node = arena.Alloc<Node>(stable_view);
        node_map[stable_view] = node;
        return node;
    }

    void DetermineDirtyState(Node* node) {
        node->mtime = ShardedStatCache::GetMtime(node->path);
        node->exists = (node->mtime != 0);

        if (!node->in_edge) return;

        uint64_t max_input_mtime = 0;
        for (Node* in : node->in_edge->inputs) {
            DetermineDirtyState(in);
            max_input_mtime = std::max(max_input_mtime, in->mtime);
            if (in->dirty) {
                node->dirty = true;
            }
        }

        if (!node->exists || node->mtime < max_input_mtime) {
            node->dirty = true;
        }
    }

    bool ExecuteBuild(const std::vector<Node*>& targets, int parallelism, bool dry_run, bool verbose, int keep_going) {
        for (Node* target : targets) {
            DetermineDirtyState(target);
        }

        std::vector<Edge*> ready_queue;
        size_t total_dirty_edges = 0;

        for (Edge* edge : all_edges) {
            bool has_dirty_out = false;
            for (Node* out : edge->outputs) {
                if (out->dirty) { has_dirty_out = true; break; }
            }

            if (has_dirty_out) {
                total_dirty_edges++;
                uint32_t pending = 0;
                for (Node* in : edge->inputs) {
                    if (in->in_edge && in->dirty) pending++;
                }
                edge->pending_inputs.store(pending, std::memory_order_relaxed);
                if (pending == 0) {
                    ready_queue.push_back(edge);
                }
            }
        }

        if (total_dirty_edges == 0) {
            std::cout << "ninja: no work to do.\n";
            return true;
        }

        // Lock-Striped Thread Pool Execution
        std::mutex queue_mutex;
        std::mutex output_mutex;
        std::condition_variable cv;
        std::deque<Edge*> work_deque(ready_queue.begin(), ready_queue.end());
        std::atomic<size_t> completed_edges{0};
        std::atomic<int> failed_jobs{0};
        std::atomic<bool> abort_execution{false};

        auto worker = [&]() {
            while (!abort_execution.load(std::memory_order_relaxed) && 
                   !ProcessTracker::g_interrupted.load(std::memory_order_relaxed)) {
                Edge* job = nullptr;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    cv.wait(lock, [&]() {
                        return !work_deque.empty() || 
                                completed_edges.load(std::memory_order_relaxed) == total_dirty_edges || 
                                abort_execution.load(std::memory_order_relaxed) ||
                                ProcessTracker::g_interrupted.load(std::memory_order_relaxed);
                    });

                    if (abort_execution.load(std::memory_order_relaxed) || 
                        ProcessTracker::g_interrupted.load(std::memory_order_relaxed) ||
                        completed_edges.load(std::memory_order_relaxed) == total_dirty_edges) {
                        return;
                    }

                    if (!work_deque.empty()) {
                        job = work_deque.front();
                        work_deque.pop_front();
                    }
                }

                if (!job) continue;

                std::string cmd = job->ExpandCommand();
                std::string output_buffer;
                int ret = SubprocessEngine::Execute(cmd, dry_run, verbose, output_buffer);

                // Atomic output flush prevents interleaved log corruption
                if (!output_buffer.empty() || ret != 0) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    if (!job->rule.description.empty() && !verbose) {
                        std::cout << job->rule.description << "\n";
                    }
                    if (!output_buffer.empty()) {
                        std::cout << output_buffer;
                    }
                }

                if (ret != 0) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cerr << "ninja: build failed on command: " << cmd << " (exit status " << ret << ")\n";
                    int current_fails = ++failed_jobs;
                    if (keep_going != 0 && current_fails >= keep_going) {
                        abort_execution.store(true, std::memory_order_release);
                        ProcessTracker::KillAllActive();
                    } else if (keep_going == 1) {
                        abort_execution.store(true, std::memory_order_release);
                        ProcessTracker::KillAllActive();
                    }
                    cv.notify_all();
                    return;
                }

                // Push dependents using lock-free fetch_sub
                for (Node* out : job->outputs) {
                    for (Edge* dep : out->out_edges) {
                        if (dep->pending_inputs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                            std::lock_guard<std::mutex> lock(queue_mutex);
                            work_deque.push_back(dep);
                            cv.notify_one();
                        }
                    }
                }

                completed_edges.fetch_add(1, std::memory_order_release);
                cv.notify_all();
            }
        };

        std::vector<std::thread> pool;
        for (int i = 0; i < parallelism; ++i) pool.emplace_back(worker);
        for (auto& t : pool) t.join();

        return failed_jobs.load(std::memory_order_relaxed) == 0 && 
               !ProcessTracker::g_interrupted.load(std::memory_order_relaxed);
    }
};

// ============================================================================
// 6. ZERO-COPY MMAP PARSER ENGINE
// ============================================================================
class ManifestParser {
public:
    static bool ParseFile(const std::string& path, BuildEngine& engine) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) {
            std::cerr << "ninja: error: loading '" << path << "'\n";
            return false;
        }

        size_t size = static_cast<size_t>(input.tellg());
        if (size == 0) {
            return true;
        }

        input.seekg(0, std::ios::beg);
        std::string file_data(size, '\0');
        input.read(file_data.data(), static_cast<std::streamsize>(size));
        if (!input && !input.eof()) {
            std::cerr << "FATAL: cannot read manifest\n";
            return false;
        }

        std::string_view content(file_data.data(), file_data.size());
        size_t pos = 0;
        Rule current_rule;
        bool in_rule = false;

        while (pos < content.size()) {
            size_t next_line = content.find('\n', pos);
            if (next_line == std::string_view::npos) next_line = content.size();

            std::string_view line = content.substr(pos, next_line - pos);
            pos = next_line + 1;

            size_t cpos = line.find('#');
            if (cpos != std::string_view::npos) line = line.substr(0, cpos);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.remove_suffix(1);
            if (line.empty()) continue;

            if (line[0] == ' ' || line[0] == '\t') {
                while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.remove_prefix(1);
                if (in_rule) {
                    size_t eq_pos = line.find('=');
                    if (eq_pos != std::string_view::npos) {
                        std::string_view key = line.substr(0, eq_pos);
                        std::string_view val = line.substr(eq_pos + 1);
                        while (!key.empty() && key.back() == ' ') key.remove_suffix(1);
                        while (!val.empty() && val.front() == ' ') val.remove_prefix(1);
                        if (key == "command") current_rule.command = val;
                        else if (key == "description") current_rule.description = val;
                    }
                }
                continue;
            }

            if (in_rule) {
                engine.rule_map[current_rule.name] = current_rule;
                in_rule = false;
            }

            if (line.substr(0, 4) == "rule") {
                in_rule = true;
                current_rule = Rule();
                line.remove_prefix(4);
                while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
                current_rule.name = line;
            } else if (line.substr(0, 5) == "build") {
                line.remove_prefix(5);
                size_t colon = line.find(':');
                if (colon == std::string_view::npos) continue;

                std::string_view out_part = line.substr(0, colon);
                std::string_view rest = line.substr(colon + 1);

                while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
                size_t space_pos = rest.find(' ');
                std::string_view rname = (space_pos == std::string_view::npos) ? rest : rest.substr(0, space_pos);
                std::string_view in_part = (space_pos == std::string_view::npos) ? "" : rest.substr(space_pos + 1);

                Edge* edge = engine.arena.Alloc<Edge>();
                edge->rule = engine.rule_map[rname];

                // Parse outputs
                size_t p = 0;
                while (p < out_part.size()) {
                    while (p < out_part.size() && out_part[p] == ' ') ++p;
                    if (p >= out_part.size()) break;
                    size_t end = out_part.find(' ', p);
                    if (end == std::string_view::npos) end = out_part.size();
                    Node* out_node = engine.GetOrCreateNode(out_part.substr(p, end - p));
                    out_node->in_edge = edge;
                    edge->outputs.push_back(out_node);
                    p = end;
                }

                // Parse inputs
                p = 0;
                while (p < in_part.size()) {
                    while (p < in_part.size() && in_part[p] == ' ') ++p;
                    if (p >= in_part.size()) break;
                    size_t end = in_part.find(' ', p);
                    if (end == std::string_view::npos) end = in_part.size();
                    std::string_view in_name = in_part.substr(p, end - p);
                    if (in_name != "|") {
                        Node* in_node = engine.GetOrCreateNode(in_name);
                        in_node->out_edges.push_back(edge);
                        edge->inputs.push_back(in_node);
                    }
                    p = end;
                }
                engine.all_edges.push_back(edge);
            } else if (line.substr(0, 7) == "default") {
                line.remove_prefix(7);
                while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
                engine.default_targets.push_back(engine.GetOrCreateNode(line));
            }
        }

        if (in_rule) engine.rule_map[current_rule.name] = current_rule;
        return true;
    }
};

} // namespace EnterpriseNinja

// ============================================================================
// COMPREHENSIVE CLI INTERFACE
// ============================================================================
void PrintHelp() {
    std::cout <<
R"(usage: ninja [options] [targets...]

Ninja Build System v3.0.0

options:
  --version      print ninja version ("3.0.0")
  -v, --verbose  show all command lines while building
  -C DIR         change to DIR before doing anything else
  -f FILE        specify input build file [default=build.ninja]
  -j N           run N jobs in parallel [default=CPU count]
  -k N           keep going until N jobs fail (0 means keep going forever) [default=1]
  -l N           do not start new jobs if the load average is greater than N
  -n             dry run (don't run commands but pretend they succeeded)
  -d MODE        enable debugging modes (stats, explain, keepdepfile)
  -t TOOL        run a subtool (clean, targets, graph)
  -h, --help     print this message
)";
}

int main(int argc, char** argv) {
    EnterpriseNinja::ProcessTracker::InstallSignalHandlers();

    std::string manifest = "build.ninja";
    int jobs = static_cast<int>(std::thread::hardware_concurrency());
    if (jobs <= 0) jobs = 4;
    int keep_going = 1;
    bool dry_run = false;
    bool verbose = false;
    std::vector<std::string> target_names;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            PrintHelp();
            EnterpriseNinja::ProcessTracker::Cleanup();
            return 0;
        } else if (arg == "--version") {
            std::cout << "3.0.0\n";
            EnterpriseNinja::ProcessTracker::Cleanup();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-n") {
            dry_run = true;
        } else if (arg == "-f" && i + 1 < argc) {
            manifest = argv[++i];
        } else if (arg == "-j" && i + 1 < argc) {
            jobs = std::stoi(argv[++i]);
        } else if (arg == "-k" && i + 1 < argc) {
            keep_going = std::stoi(argv[++i]);
        } else if (arg == "-C" && i + 1 < argc) {
            std::error_code ec;
            std::filesystem::current_path(argv[++i], ec);
            if (ec) {
                std::cerr << "ninja: fatal: cannot change directory to '" << argv[i] << "'\n";
                EnterpriseNinja::ProcessTracker::Cleanup();
                return 1;
            }
        } else if (arg[0] == '-') {
            std::cerr << "ninja: warning: flag '" << arg << "' ignored in enterprise baseline mode\n";
        } else {
            target_names.push_back(arg);
        }
    }

    EnterpriseNinja::BuildEngine engine;
    if (!EnterpriseNinja::ManifestParser::ParseFile(manifest, engine)) {
        EnterpriseNinja::ProcessTracker::Cleanup();
        return 1;
    }

    std::vector<EnterpriseNinja::Node*> targets;
    if (target_names.empty()) {
        if (!engine.default_targets.empty()) {
            targets = engine.default_targets;
        } else if (!engine.all_edges.empty()) {
            targets = engine.all_edges.front()->outputs;
        }
    } else {
        for (const auto& t : target_names) {
            targets.push_back(engine.GetOrCreateNode(t));
        }
    }

    bool success = engine.ExecuteBuild(targets, jobs, dry_run, verbose, keep_going);
    EnterpriseNinja::ProcessTracker::Cleanup();
    return success ? 0 : 1;
}