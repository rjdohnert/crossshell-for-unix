#include "jobs.hpp"
#include "engine.hpp"
#include "scripting.hpp"

void write_binary_string(ofstream& output, const string& value) {
    uint64_t size = static_cast<uint64_t>(value.size());
    output.write(reinterpret_cast<const char*>(&size), sizeof(size));
    output.write(value.data(), static_cast<streamsize>(value.size()));
}

bool read_binary_string(ifstream& input, string& value) {
    uint64_t size = 0;
    if (!input.read(reinterpret_cast<char*>(&size), sizeof(size)) || size > 16 * 1024 * 1024) return false;
    value.resize(static_cast<size_t>(size));
    return size == 0 || static_cast<bool>(input.read(value.data(), static_cast<streamsize>(size)));
}

unsigned long g_next_job_id = 1;
unsigned long g_current_job_id = 0;
unsigned long g_previous_job_id = 0;

void select_current_job(unsigned long job_id) {
    if (job_id == 0 || job_id == g_current_job_id) return;
    g_previous_job_id = g_current_job_id;
    g_current_job_id = job_id;
}

void repair_job_markers() {
    auto exists = [](unsigned long id) {
        return id != 0 && any_of(g_env.jobs.begin(), g_env.jobs.end(),
                                 [&](const BackgroundJob& job) { return job.job_id == id; });
    };
    if (!exists(g_current_job_id)) {
        g_current_job_id = exists(g_previous_job_id) ? g_previous_job_id :
            (g_env.jobs.empty() ? 0 : g_env.jobs.back().job_id);
    }
    if (!exists(g_previous_job_id) || g_previous_job_id == g_current_job_id) {
        g_previous_job_id = 0;
        for (auto it = g_env.jobs.rbegin(); it != g_env.jobs.rend(); ++it) {
            if (it->job_id != g_current_job_id) { g_previous_job_id = it->job_id; break; }
        }
    }
}

bool save_pipeline_shell_state(const string& path) {
    ofstream output(normalize_path_to_win(path), ios::binary | ios::trunc);
    if (!output) return false;
    const char magic[] = "ZSHPIPE2";
    output.write(magic, sizeof(magic));

    auto write_string_map = [&](const auto& values) {
        uint64_t count = static_cast<uint64_t>(values.size());
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& [name, value] : values) {
            write_binary_string(output, name);
            write_binary_string(output, value);
        }
    };
    auto write_string_set = [&](const set<string>& values) {
        uint64_t count = static_cast<uint64_t>(values.size());
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& value : values) write_binary_string(output, value);
    };
    auto write_string_vector = [&](const vector<string>& values) {
        uint64_t count = static_cast<uint64_t>(values.size());
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& value : values) write_binary_string(output, value);
    };
    write_string_map(g_env.vars);
    write_string_map(g_env.functions);
    write_string_map(g_env.aliases);

    uint64_t option_count = static_cast<uint64_t>(g_env.options.size());
    output.write(reinterpret_cast<const char*>(&option_count), sizeof(option_count));
    for (const auto& [name, enabled] : g_env.options) {
        write_binary_string(output, name);
        uint8_t value = enabled ? 1 : 0;
        output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    uint64_t array_count = static_cast<uint64_t>(g_env.indexed_arrays.size());
    output.write(reinterpret_cast<const char*>(&array_count), sizeof(array_count));
    for (const auto& [name, values] : g_env.indexed_arrays) {
        write_binary_string(output, name);
        uint64_t value_count = static_cast<uint64_t>(values.size());
        output.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));
        for (const auto& value : values) write_binary_string(output, value);
    }

    uint64_t assoc_count = static_cast<uint64_t>(g_env.assoc_arrays.size());
    output.write(reinterpret_cast<const char*>(&assoc_count), sizeof(assoc_count));
    for (const auto& [name, values] : g_env.assoc_arrays) {
        write_binary_string(output, name);
        write_string_map(values);
    }
    write_string_set(g_env.unique_arrays);
    write_string_set(g_env.integer_vars);
    write_string_set(g_env.readonly_vars);
    write_string_set(g_env.autoload_functions);
    write_string_set(g_env.loaded_modules);
    write_string_vector(g_env.positional_args);
    write_string_map(g_env.completion_definitions);
    write_string_map(g_env.widgets);
    return static_cast<bool>(output);
}

bool load_pipeline_shell_state(const string& path) {
    ifstream input(normalize_path_to_win(path), ios::binary);
    char magic[9] = {};
    if (!input.read(magic, sizeof(magic)) || memcmp(magic, "ZSHPIPE2", sizeof(magic)) != 0) return false;

    auto read_string_map = [&](auto& values) {
        uint64_t count = 0;
        if (!input.read(reinterpret_cast<char*>(&count), sizeof(count)) || count > 100000) return false;
        values.clear();
        for (uint64_t i = 0; i < count; ++i) {
            string name, value;
            if (!read_binary_string(input, name) || !read_binary_string(input, value)) return false;
            values[std::move(name)] = std::move(value);
        }
        return true;
    };
    auto read_string_set = [&](set<string>& values) {
        uint64_t count = 0;
        if (!input.read(reinterpret_cast<char*>(&count), sizeof(count)) || count > 100000) return false;
        values.clear();
        for (uint64_t i = 0; i < count; ++i) {
            string value;
            if (!read_binary_string(input, value)) return false;
            values.insert(std::move(value));
        }
        return true;
    };
    auto read_string_vector = [&](vector<string>& values) {
        uint64_t count = 0;
        if (!input.read(reinterpret_cast<char*>(&count), sizeof(count)) || count > 1000000) return false;
        values.clear();
        values.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) {
            string value;
            if (!read_binary_string(input, value)) return false;
            values.push_back(std::move(value));
        }
        return true;
    };
    if (!read_string_map(g_env.vars) || !read_string_map(g_env.functions) || !read_string_map(g_env.aliases)) return false;

    uint64_t option_count = 0;
    if (!input.read(reinterpret_cast<char*>(&option_count), sizeof(option_count)) || option_count > 10000) return false;
    g_env.options.clear();
    for (uint64_t i = 0; i < option_count; ++i) {
        string name;
        uint8_t enabled = 0;
        if (!read_binary_string(input, name) || !input.read(reinterpret_cast<char*>(&enabled), sizeof(enabled))) return false;
        g_env.options[std::move(name)] = enabled != 0;
    }

    uint64_t array_count = 0;
    if (!input.read(reinterpret_cast<char*>(&array_count), sizeof(array_count)) || array_count > 100000) return false;
    g_env.indexed_arrays.clear();
    for (uint64_t i = 0; i < array_count; ++i) {
        string name;
        uint64_t value_count = 0;
        if (!read_binary_string(input, name) ||
            !input.read(reinterpret_cast<char*>(&value_count), sizeof(value_count)) || value_count > 1000000) return false;
        vector<string> values;
        values.reserve(static_cast<size_t>(value_count));
        for (uint64_t value_index = 0; value_index < value_count; ++value_index) {
            string value;
            if (!read_binary_string(input, value)) return false;
            values.push_back(std::move(value));
        }
        g_env.indexed_arrays[std::move(name)] = std::move(values);
    }

    uint64_t assoc_count = 0;
    if (!input.read(reinterpret_cast<char*>(&assoc_count), sizeof(assoc_count)) || assoc_count > 100000) return false;
    g_env.assoc_arrays.clear();
    for (uint64_t i = 0; i < assoc_count; ++i) {
        string name;
        map<string, string> values;
        if (!read_binary_string(input, name) || !read_string_map(values)) return false;
        g_env.assoc_arrays[std::move(name)] = std::move(values);
    }
    return read_string_set(g_env.unique_arrays) &&
           read_string_set(g_env.integer_vars) &&
           read_string_set(g_env.readonly_vars) &&
           read_string_set(g_env.autoload_functions) &&
           read_string_set(g_env.loaded_modules) &&
           read_string_vector(g_env.positional_args) &&
           read_string_map(g_env.completion_definitions) &&
           read_string_map(g_env.widgets);
}

CoprocessState g_coprocess;

void close_coprocess(bool terminate_if_running) {
    close_handle_if_valid(g_coprocess.input);
    close_handle_if_valid(g_coprocess.output);
    g_coprocess.input = nullptr;
    g_coprocess.output = nullptr;

    if (g_coprocess.process) {
        DWORD exit_code = 0;
        if (terminate_if_running && GetExitCodeProcess(g_coprocess.process, &exit_code) && exit_code == STILL_ACTIVE) {
            TerminateProcess(g_coprocess.process, 1);
        }
        CloseHandle(g_coprocess.process);
        g_coprocess.process = nullptr;
    }
    g_coprocess.pid = 0;
    g_env.vars.erase("COPROC_PID");
}

atomic<bool> g_sigint_pending{false};
atomic<bool> g_sigwinch_pending{false};
atomic<bool> g_sigtstp_pending{false};
mutex g_foreground_pids_mutex;
vector<DWORD> g_foreground_pids;

void set_foreground_pids(const vector<DWORD>& pids) {
    lock_guard<mutex> lock(g_foreground_pids_mutex);
    g_foreground_pids = pids;
}

void clear_foreground_pids() {
    lock_guard<mutex> lock(g_foreground_pids_mutex);
    g_foreground_pids.clear();
}

BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        g_sigint_pending.store(true);
        return TRUE; // handled; process trap on main thread
    }
    return FALSE;
}

#ifndef PROCESS_SUSPEND_RESUME
#define PROCESS_SUSPEND_RESUME 0x0800
#endif

void suspend_win32_process(DWORD pid) {
    typedef LONG (NTAPI *NtSuspendProcPtr)(HANDLE ProcessHandle);
    static NtSuspendProcPtr NtSuspendProcess =
        (NtSuspendProcPtr)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSuspendProcess");

    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (hProcess) {
        if (NtSuspendProcess) {
            NtSuspendProcess(hProcess);
        } else {
            // Fallback: suspend all threads in target process.
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (hSnap != INVALID_HANDLE_VALUE) {
                THREADENTRY32 te{ sizeof(THREADENTRY32) };
                if (Thread32First(hSnap, &te)) {
                    do {
                        if (te.th32OwnerProcessID == pid) {
                            HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                            if (hThread) {
                                SuspendThread(hThread);
                                CloseHandle(hThread);
                            }
                        }
                    } while (Thread32Next(hSnap, &te));
                }
                CloseHandle(hSnap);
            }
        }
        CloseHandle(hProcess);
    }
}

bool resume_win32_process(DWORD pid) {
    typedef LONG (NTAPI *NtResumeProcPtr)(HANDLE ProcessHandle);
    static NtResumeProcPtr NtResumeProcess =
        (NtResumeProcPtr)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtResumeProcess");
    HANDLE process = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!process) return false;
    bool resumed = false;
    if (NtResumeProcess) {
        resumed = NtResumeProcess(process) >= 0;
    } else {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 entry{sizeof(THREADENTRY32)};
            if (Thread32First(snapshot, &entry)) {
                do {
                    if (entry.th32OwnerProcessID != pid) continue;
                    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
                    if (!thread) continue;
                    while (ResumeThread(thread) > 1) {}
                    CloseHandle(thread);
                    resumed = true;
                } while (Thread32Next(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }
    }
    CloseHandle(process);
    return resumed;
}

bool suspend_active_foreground_processes() {
    vector<DWORD> pids;
    {
        lock_guard<mutex> lock(g_foreground_pids_mutex);
        pids = g_foreground_pids;
    }
    if (pids.empty()) return false;
    for (DWORD pid : pids) suspend_win32_process(pid);
    return true;
}

void check_window_resize_event() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD num_events = 0;
    if (GetNumberOfConsoleInputEvents(hIn, &num_events) && num_events > 0) {
        vector<INPUT_RECORD> records(num_events);
        DWORD read_count = 0;
        if (PeekConsoleInputW(hIn, records.data(), num_events, &read_count)) {
            for (DWORD i = 0; i < read_count; ++i) {
                if (records[i].EventType == WINDOW_BUFFER_SIZE_EVENT) {
                    g_sigwinch_pending.store(true);
                    break;
                }
            }
        }
    }
}

void init_signal_handlers() {
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
}

void process_pending_traps() {
    // A. SIGINT
    if (g_sigint_pending.exchange(false)) {
        if (g_env.vars.count("__trap_INT")) {
            execute_command_line(g_env.vars["__trap_INT"]);
        } else if (g_env.vars.count("__trap_SIGINT")) {
            execute_command_line(g_env.vars["__trap_SIGINT"]);
        } else if (g_env.vars.count("__trap_2")) {
            execute_command_line(g_env.vars["__trap_2"]);
        }
    }

    // B. SIGWINCH
    if (g_sigwinch_pending.exchange(false)) {
        g_env.prompt_dirty = true;
        if (g_env.vars.count("__trap_WINCH")) {
            execute_command_line(g_env.vars["__trap_WINCH"]);
        } else if (g_env.vars.count("__trap_SIGWINCH")) {
            execute_command_line(g_env.vars["__trap_SIGWINCH"]);
        } else if (g_env.vars.count("__trap_28")) {
            execute_command_line(g_env.vars["__trap_28"]);
        }
    }

    // C. SIGTSTP
    if (g_sigtstp_pending.exchange(false)) {
        if (g_env.vars.count("__trap_TSTP")) {
            execute_command_line(g_env.vars["__trap_TSTP"]);
        } else if (g_env.vars.count("__trap_SIGTSTP")) {
            execute_command_line(g_env.vars["__trap_SIGTSTP"]);
        } else if (g_env.vars.count("__trap_20")) {
            execute_command_line(g_env.vars["__trap_20"]);
        }
    }
}

bool g_exit_trap_fired = false;

void fire_exit_trap() {
    if (g_exit_trap_fired) return;
    g_exit_trap_fired = true;
    if (g_env.vars.count("__trap_EXIT")) {
        string trap_body = g_env.vars["__trap_EXIT"];
        int rc = execute_command_line(trap_body);
        cout.flush();
        cerr.flush();
        (void)rc;
    }
}

int execute_pipeline_native(const Pipeline& pl) {
    if (pl.cmds.empty()) return 0;
    size_t num_cmds = pl.cmds.size();

    vector<HANDLE> hPipesRead(num_cmds - 1, NULL), hPipesWrite(num_cmds - 1, NULL);
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    for (size_t i = 0; i < num_cmds - 1; ++i) {
        if (!CreatePipe(&hPipesRead[i], &hPipesWrite[i], &sa, 0)) {
            cerr << COLOR_RED << "zsh: failed to create pipeline" << COLOR_RESET << "\n";
            for (size_t j = 0; j < i; ++j) {
                if (hPipesRead[j]) CloseHandle(hPipesRead[j]);
                if (hPipesWrite[j]) CloseHandle(hPipesWrite[j]);
            }
            return 1;
        }
    }

    vector<PROCESS_INFORMATION> pi_list(num_cmds);
    vector<unique_handle> proc_handles;
    vector<unique_handle> thread_handles;
    proc_handles.reserve(num_cmds);
    thread_handles.reserve(num_cmds);
    for (size_t i = 0; i < num_cmds; ++i) {
        proc_handles.push_back(make_unique_handle());
        thread_handles.push_back(make_unique_handle());
    }
    unique_handle background_job = make_unique_handle(pl.background ? CreateJobObjectW(nullptr, nullptr) : nullptr);
    if (pl.background && !background_job.get()) {
        DWORD error = GetLastError();
        for (size_t i = 0; i + 1 < num_cmds; ++i) {
            close_handle_if_valid(hPipesRead[i]);
            close_handle_if_valid(hPipesWrite[i]);
        }
        cerr << "zsh: failed to create background job object (error " << error << ")\n";
        return 1;
    }
    bool launch_failed = false;
    DWORD launch_error = ERROR_SUCCESS;

    for (size_t i = 0; i < num_cmds; ++i) {
        SingleCmd cmd = pl.cmds[i];

        string exe_path = find_executable_in_path(cmd.args[0]);
        bool is_script = !exe_path.empty() &&
                         (exe_path.size() > 4) &&
                         (_stricmp(exe_path.c_str() + exe_path.size() - 4, ".bat") == 0 ||
                          _stricmp(exe_path.c_str() + exe_path.size() - 4, ".cmd") == 0);

        if (exe_path.empty()) {
            cerr << COLOR_RED << "zsh: command not found: " << cmd.args[0] << COLOR_RESET << "\n";
            launch_failed = true;
            launch_error = ERROR_FILE_NOT_FOUND;
            break;
        }

        bool use_cmd_fallback = is_script;
        bool is_cmd_exe = false;
        if (!exe_path.empty()) {
            string exe_name = fs::path(exe_path).filename().string();
            is_cmd_exe = (_stricmp(exe_name.c_str(), "cmd.exe") == 0);
        }

        // Wrap a single token in cmd-native double-quotes if it contains spaces or quotes.
        auto cmd_quote_arg = [](const string& s) -> string {
            if (s.find_first_of(" \t\"") == string::npos) return s;
            string r = "\"";
            for (char c : s) { if (c == '"') r += "\"\""; else r += c; }
            return r + "\"";
        };

        string cmdline;
        if (!use_cmd_fallback) {
            if (is_cmd_exe && cmd.args.size() >= 3 &&
                (_stricmp(cmd.args[1].c_str(), "/c") == 0 || _stricmp(cmd.args[1].c_str(), "/k") == 0)) {
                // cmd.exe expects one command tail after /c or /k, not split argv segments.
                string tail;
                for (size_t k = 2; k < cmd.args.size(); ++k) {
                    if (!tail.empty()) tail += " ";
                    tail += cmd.args[k];
                }
                cmdline = win_quote_arg(exe_path) + " " + cmd.args[1] + " " + cmd_quote_arg(tail);
            } else {
            // Direct execution for all PATH-resolved .exe / .com binaries.
                cmdline = win_quote_arg(exe_path);
                for (size_t k = 1; k < cmd.args.size(); ++k) cmdline += " " + win_quote_arg(cmd.args[k]);
            }
        } else {
            // cmd.exe /c: use cmd-native quoting, NOT win_quote_arg (which uses CRT rules).
            string target = is_script ? exe_path : cmd.args[0];
            string inner = cmd_quote_arg(target);
            for (size_t k = 1; k < cmd.args.size(); ++k) inner += " " + cmd_quote_arg(cmd.args[k]);
            cmdline = "cmd.exe /c " + inner;
        }

        wstring wcmd = string_to_wstring(cmdline);
        if (wcmd.empty()) continue; // skip if cmdline construction produced nothing
        STARTUPINFOW si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES;

        unique_handle hFileIn = make_unique_handle();
        unique_handle hFileOut = make_unique_handle();
        unique_handle hFileErr = make_unique_handle();
        unique_handle hHereRead = make_unique_handle();
        unique_handle hHereWrite = make_unique_handle();
        unique_handle hNullIn = make_unique_handle();
        unique_handle hNullOut = make_unique_handle();
        unique_handle hNullErr = make_unique_handle();
        bool launch_blocked = false;

        if (!cmd.input_file.empty() && g_heredoc_payloads.count(cmd.input_file)) {
            HANDLE rawHereRead = nullptr;
            HANDLE rawHereWrite = nullptr;
            if (CreatePipe(&rawHereRead, &rawHereWrite, &sa, 0)) {
                hHereRead = make_unique_handle(rawHereRead);
                hHereWrite = make_unique_handle(rawHereWrite);
                const string payload = g_heredoc_payloads[cmd.input_file];
                DWORD total_written = 0;
                while (total_written < (DWORD)payload.size()) {
                    DWORD written = 0;
                    if (!WriteFile((HANDLE)hHereWrite.get(), payload.data() + total_written,
                                   (DWORD)payload.size() - total_written, &written, NULL) || written == 0) break;
                    total_written += written;
                }
                hHereWrite.reset();
                si.hStdInput = (HANDLE)hHereRead.get();
            } else {
                si.hStdInput = (i > 0) ? hPipesRead[i - 1] : GetStdHandle(STD_INPUT_HANDLE);
            }
            g_heredoc_payloads.erase(cmd.input_file);
        } else if (!cmd.input_file.empty()) {
            hFileIn = make_unique_handle(CreateFileW(string_to_wstring(normalize_path_to_win(cmd.input_file)).c_str(), GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (!hFileIn.get()) {
                cerr << COLOR_RED << "zsh: cannot open input file: " << cmd.input_file << COLOR_RESET << "\n";
                launch_blocked = true;
            } else {
                si.hStdInput = (HANDLE)hFileIn.get();
            }
        } else if (!cmd.here_string.empty()) {
            HANDLE rawHereRead = nullptr;
            HANDLE rawHereWrite = nullptr;
            if (CreatePipe(&rawHereRead, &rawHereWrite, &sa, 0)) {
                hHereRead = make_unique_handle(rawHereRead);
                hHereWrite = make_unique_handle(rawHereWrite);
                string payload = cmd.here_string + "\n";
                DWORD total_written = 0;
                while (total_written < (DWORD)payload.size()) {
                    DWORD written = 0;
                    if (!WriteFile((HANDLE)hHereWrite.get(), payload.data() + total_written,
                                   (DWORD)payload.size() - total_written, &written, NULL) || written == 0) break;
                    total_written += written;
                }
                hHereWrite.reset();
                si.hStdInput = (HANDLE)hHereRead.get();
            } else {
                si.hStdInput = (i > 0) ? hPipesRead[i - 1] : GetStdHandle(STD_INPUT_HANDLE);
            }
        } else si.hStdInput = (i > 0) ? hPipesRead[i - 1] : GetStdHandle(STD_INPUT_HANDLE);

        if (!cmd.output_file.empty()) {
            DWORD creation = cmd.append_out ? OPEN_ALWAYS : CREATE_ALWAYS;
            hFileOut = make_unique_handle(CreateFileW(string_to_wstring(normalize_path_to_win(cmd.output_file)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, &sa, creation, FILE_ATTRIBUTE_NORMAL, NULL));
            if (!hFileOut.get()) {
                cerr << COLOR_RED << "zsh: cannot open output file: " << cmd.output_file << COLOR_RESET << "\n";
                launch_blocked = true;
            } else {
                if (cmd.append_out &&
                    SetFilePointer((HANDLE)hFileOut.get(), 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR) {
                    cerr << COLOR_RED << "zsh: cannot seek to end of output file: " << cmd.output_file << COLOR_RESET << "\n";
                    launch_blocked = true;
                } else {
                    si.hStdOutput = (HANDLE)hFileOut.get();
                }
            }
        } else si.hStdOutput = (i < num_cmds - 1) ? hPipesWrite[i] : GetStdHandle(STD_OUTPUT_HANDLE);

        if (!cmd.error_file.empty()) {
            DWORD creation = cmd.append_err ? OPEN_ALWAYS : CREATE_ALWAYS;
            hFileErr = make_unique_handle(CreateFileW(string_to_wstring(normalize_path_to_win(cmd.error_file)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, &sa, creation, FILE_ATTRIBUTE_NORMAL, NULL));
            if (!hFileErr.get()) {
                cerr << COLOR_RED << "zsh: cannot open error file: " << cmd.error_file << COLOR_RESET << "\n";
                launch_blocked = true;
            } else {
                if (cmd.append_err &&
                    SetFilePointer((HANDLE)hFileErr.get(), 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR) {
                    cerr << COLOR_RED << "zsh: cannot seek to end of error file: " << cmd.error_file << COLOR_RESET << "\n";
                    launch_blocked = true;
                } else {
                    si.hStdError = (HANDLE)hFileErr.get();
                }
            }
        } else si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        if (launch_blocked) {
            launch_failed = true;
            launch_error = GetLastError();
            break;
        }

        if (cmd.close_stdin) {
            hNullIn = make_unique_handle(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (hNullIn.get()) si.hStdInput = (HANDLE)hNullIn.get();
        }
        if (cmd.close_stdout) {
            hNullOut = make_unique_handle(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (hNullOut.get()) si.hStdOutput = (HANDLE)hNullOut.get();
        }
        if (cmd.close_stderr) {
            hNullErr = make_unique_handle(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (hNullErr.get()) si.hStdError = (HANDLE)hNullErr.get();
        }

        auto fd_handle = [&](int fd) -> HANDLE {
            if (fd == 0) return si.hStdInput;
            if (fd == 1) return si.hStdOutput;
            if (fd == 2) return si.hStdError;
            return INVALID_HANDLE_VALUE;
        };
        if (cmd.dup_stdout_from >= 0) {
            HANDLE src = fd_handle(cmd.dup_stdout_from);
            if (src != INVALID_HANDLE_VALUE) si.hStdOutput = src;
        }
        if (cmd.dup_stderr_from >= 0) {
            HANDLE src = fd_handle(cmd.dup_stderr_from);
            if (src != INVALID_HANDLE_VALUE) si.hStdError = src;
        }
        if (cmd.dup_stdin_from >= 0) {
            HANDLE src = fd_handle(cmd.dup_stdin_from);
            if (src != INVALID_HANDLE_VALUE) si.hStdInput = src;
        }

        ZeroMemory(&pi_list[i], sizeof(PROCESS_INFORMATION));
        DWORD creation_flags = pl.background ? CREATE_SUSPENDED : 0;
        if (!CreateProcessW(NULL, &wcmd[0], NULL, NULL, TRUE, creation_flags, NULL, NULL, &si, &pi_list[i])) {
            launch_error = GetLastError();
            cerr << COLOR_RED << "zsh: " << cmd.args[0] << ": command not found" << COLOR_RESET << "\n";
            launch_failed = true;
            break;
        }
        proc_handles[i] = make_unique_handle(pi_list[i].hProcess);
        thread_handles[i] = make_unique_handle(pi_list[i].hThread);
        if (background_job.get()) {
            if (!AssignProcessToJobObject((HANDLE)background_job.get(), pi_list[i].hProcess)) {
                launch_error = GetLastError();
                launch_failed = true;
            } else if (ResumeThread(pi_list[i].hThread) == static_cast<DWORD>(-1)) {
                launch_error = GetLastError();
                launch_failed = true;
            }
        }

        // Close parent's copies of inherited file handles; child retains its own copies.
        hFileIn.reset();
        hFileOut.reset();
        hFileErr.reset();
        hHereRead.reset();
        hHereWrite.reset();
        hNullIn.reset();
        hNullOut.reset();
        hNullErr.reset();
        if (i > 0) { CloseHandle(hPipesRead[i - 1]); hPipesRead[i - 1] = nullptr; }
        if (i < num_cmds - 1) { CloseHandle(hPipesWrite[i]); hPipesWrite[i] = nullptr; }
        if (launch_failed) break;
    }

    if (launch_failed) {
        if (background_job.get()) {
            TerminateJobObject((HANDLE)background_job.get(), launch_error == ERROR_SUCCESS ? 1 : launch_error);
        } else {
            for (auto& process : proc_handles)
                if (process.get()) TerminateProcess((HANDLE)process.get(), 1);
        }
        for (size_t i = 0; i + 1 < num_cmds; ++i) {
            if (hPipesRead[i]) { CloseHandle(hPipesRead[i]); hPipesRead[i] = nullptr; }
            if (hPipesWrite[i]) { CloseHandle(hPipesWrite[i]); hPipesWrite[i] = nullptr; }
        }
        for (auto& process : proc_handles)
            if (process.get()) WaitForSingleObject((HANDLE)process.get(), 5000);
        return 1;
    }

    if (pl.background) {
        vector<HANDLE> job_processes;
        vector<DWORD> job_pids;
        for (size_t i = 0; i < num_cmds; ++i) {
            thread_handles[i].reset();
            if (proc_handles[i].get()) {
                job_processes.push_back((HANDLE)proc_handles[i].release());
                job_pids.push_back(pi_list[i].dwProcessId);
            }
        }
        HANDLE job_process = job_processes.empty() ? nullptr : job_processes.back();
        HANDLE job_object = (HANDLE)background_job.release();
        BackgroundJob job;
        job.pid = pi_list.back().dwProcessId;
        job.hProcess = job_process;
        job.hJob = job_object;
        job.processes = std::move(job_processes);
        job.pids = std::move(job_pids);
        job.command = pl.cmds[0].args[0];
        job.job_id = g_next_job_id++;
        unsigned long job_id = job.job_id;
        g_env.jobs.push_back(std::move(job));
        select_current_job(job_id);
        cout << "[" << job_id << "] " << pi_list.back().dwProcessId << "\n";
        return 0;
    }

    vector<DWORD> fg_pids;
    fg_pids.reserve(num_cmds);
    for (size_t i = 0; i < num_cmds; ++i) {
        if (proc_handles[i].get() && pi_list[i].dwProcessId != 0) fg_pids.push_back(pi_list[i].dwProcessId);
    }
    set_foreground_pids(fg_pids);
    struct ForegroundPidsScopeGuard {
        ~ForegroundPidsScopeGuard() { clear_foreground_pids(); }
    } fg_scope_guard;

    vector<DWORD> exit_codes(num_cmds, 127);
    for (size_t i = 0; i < num_cmds; ++i) {
        if (!proc_handles[i].get()) continue;
        WaitForSingleObject((HANDLE)proc_handles[i].get(), INFINITE);
        if (!GetExitCodeProcess((HANDLE)proc_handles[i].get(), &exit_codes[i])) exit_codes[i] = 127;
    }
    string pipestatus;
    vector<string> pipestatus_vec;
    for (size_t i = 0; i < exit_codes.size(); ++i) {
        if (i > 0) pipestatus += ' ';
        pipestatus += to_string(exit_codes[i]);
        pipestatus_vec.push_back(to_string(exit_codes[i]));
    }
    g_env.vars["pipestatus"] = pipestatus;
    g_env.indexed_arrays["pipestatus"] = pipestatus_vec;

    if (g_env.options.count("pipefail") && g_env.options.at("pipefail")) {
        for (size_t i = exit_codes.size(); i > 0; --i)
            if (exit_codes[i - 1] != 0) return static_cast<int>(exit_codes[i - 1]);
    }
    return static_cast<int>(exit_codes.back());
}
