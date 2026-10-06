#include "engine.hpp"

namespace EnterpriseNinja {

// --- ProcessTracker Implementation ---
void ProcessTracker::Initialize() {
    g_job = CreateJobObjectW(NULL, NULL);
    if (g_job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
}

void ProcessTracker::Cleanup() {
    if (g_job) {
        CloseHandle(g_job);
        g_job = NULL;
    }
}

void ProcessTracker::RegisterProcess(HANDLE hProcess) {
    if (g_job && hProcess) {
        AssignProcessToJobObject(g_job, hProcess);
    }
}

void ProcessTracker::UnregisterProcess(HANDLE hProcess) {
    (void)hProcess;
}

void ProcessTracker::KillAllActive() {
    if (g_job) {
        TerminateJobObject(g_job, 1);
    }
}

BOOL WINAPI ProcessTracker::ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        g_interrupted.store(true, std::memory_order_release);
        KillAllActive();
        const char msg[] = "\nninja: interrupted by user. Killing child processes...\n";
        std::cerr << msg;
        return TRUE;
    }
    return FALSE;
}

void ProcessTracker::InstallSignalHandlers() {
    Initialize();
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
}

// --- ShardedStatCache Implementation ---
uint64_t ShardedStatCache::GetMtime(std::string_view path) {
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

// --- SubprocessEngine Implementation ---
bool SubprocessEngine::HasShellMetachars(std::string_view cmd) {
    return cmd.find_first_of("|&;><*?$`\n()[]{}") != std::string_view::npos;
}

int SubprocessEngine::Execute(const std::string& cmd, bool dry_run, bool verbose, std::string& output_buffer) {
    if (verbose || dry_run) {
        std::cout << cmd << "\n";
    }
    if (dry_run) return 0;

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

    BOOL bSuccess = CreateProcessW(
        NULL,
        &wcmd[0],
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &siStartInfo,
        &piProcInfo
    );

    CloseHandle(hChildStd_OUT_Wr);

    if (!bSuccess) {
        CloseHandle(hChildStd_OUT_Rd);
        return -1;
    }

    ProcessTracker::RegisterProcess(piProcInfo.hProcess);

    char buf[4096];
    DWORD dwRead;
    while (ReadFile(hChildStd_OUT_Rd, buf, sizeof(buf), &dwRead, NULL) && dwRead > 0) {
        output_buffer.append(buf, dwRead);
    }
    CloseHandle(hChildStd_OUT_Rd);

    DWORD exitCode = 0;
    WaitForSingleObject(piProcInfo.hProcess, INFINITE);
    GetExitCodeProcess(piProcInfo.hProcess, &exitCode);

    ProcessTracker::UnregisterProcess(piProcInfo.hProcess);

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    return static_cast<int>(exitCode);
}

// --- BuildEngine Implementation ---
Node* BuildEngine::GetOrCreateNode(std::string_view path) {
    auto it = node_map.find(path);
    if (it != node_map.end()) return it->second;

    char* cloned_path = arena.DuplicateString(path);
    std::string_view stable_view(cloned_path, path.size());
    Node* node = arena.Alloc<Node>(stable_view);
    node_map[stable_view] = node;
    return node;
}

void BuildEngine::DetermineDirtyState(Node* node) {
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

bool BuildEngine::ExecuteBuild(const std::vector<Node*>& targets, int parallelism, bool dry_run, bool verbose, int keep_going) {
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

// --- ManifestParser Implementation ---
bool ManifestParser::ParseFile(const std::string& path, BuildEngine& engine) {
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

} // namespace EnterpriseNinja
