#pragma once

#include "ulimit.hpp"

class UlimitOptions {
public:
    bool show_help = false;
    bool show_version = false;
    bool show_all = false;
    bool parse_error = false;
    bool kill_on_close = false;
    LimitMode mode = LimitMode::Both;
    DWORD priority_class = 0;

    std::optional<unsigned long long> core_size_blocks;
    std::optional<unsigned long long> data_seg_kb;
    std::optional<unsigned long long> nice_priority;
    std::optional<unsigned long long> file_size_blocks;
    std::optional<unsigned long long> pending_signals;
    std::optional<unsigned long long> locked_mem_kb;
    std::optional<unsigned long long> memory_megabytes;
    std::optional<unsigned long long> open_files;
    std::optional<unsigned long long> pipe_size_blocks;
    std::optional<unsigned long long> msg_queue_bytes;
    std::optional<unsigned long long> rt_priority;
    std::optional<unsigned long long> stack_size_kb;
    std::optional<unsigned long long> cpu_seconds;
    std::optional<DWORD> process_count;
    std::optional<unsigned long long> virtual_mem_mb;
    std::optional<unsigned long long> file_locks;
    std::optional<unsigned long long> working_set_mb;

    std::vector<std::wstring> command;
    OutputFormat output_format = OutputFormat::Text;
    std::wstring pipe_command;

    void Parse(int argc, wchar_t* argv[]);
};
