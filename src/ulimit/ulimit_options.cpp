#include "format_helper.hpp"
#include "ulimit_options.hpp"

void UlimitOptions::Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] != nullptr ? argv[i] : L"";
            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") { show_help = true; continue; }
            if (arg == L"-V" || arg == L"--version") { show_version = true; continue; }
            if (arg == L"-a" || arg == L"--all") { show_all = true; continue; }
            if (arg == L"-S" || arg == L"--soft") { mode = LimitMode::Soft; continue; }
            if (arg == L"-H" || arg == L"--hard") { mode = LimitMode::Hard; continue; }
            if (arg == L"--kill-on-close") { kill_on_close = true; continue; }
            if (arg == L"--json" || arg == L"-j") { output_format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { output_format = OutputFormat::Csv; continue; }
            if (arg == L"--tsv") { output_format = OutputFormat::Tsv; continue; }
            if (arg == L"--table") { output_format = OutputFormat::Table; continue; }
            if (arg == L"--output" && i + 1 < argc) {
                std::wstring fmt = argv[++i];
                if (fmt == L"json") output_format = OutputFormat::Json;
                else if (fmt == L"csv") output_format = OutputFormat::Csv;
                else if (fmt == L"tsv") output_format = OutputFormat::Tsv;
                else if (fmt == L"table") output_format = OutputFormat::Table;
                continue;
            }
            if (arg == L"--pipe" && i + 1 < argc) { pipe_command = argv[++i]; continue; }

            if (arg == L"--priority" && i + 1 < argc) {
                std::wstring pri = argv[++i];
                if (pri == L"idle") priority_class = IDLE_PRIORITY_CLASS;
                else if (pri == L"below_normal" || pri == L"below-normal") priority_class = BELOW_NORMAL_PRIORITY_CLASS;
                else if (pri == L"normal") priority_class = NORMAL_PRIORITY_CLASS;
                else if (pri == L"above_normal" || pri == L"above-normal") priority_class = ABOVE_NORMAL_PRIORITY_CLASS;
                else if (pri == L"high") priority_class = HIGH_PRIORITY_CLASS;
                else if (pri == L"realtime") priority_class = REALTIME_PRIORITY_CLASS;
                continue;
            }

            auto parse_opt = [&](std::optional<unsigned long long>& opt_val, const wchar_t* opt_name) -> bool {
                if (i + 1 >= argc || (argv[i + 1][0] == L'-' && !std::iswdigit(argv[i + 1][1]))) {
                    opt_val = 0;
                    return true;
                }
                unsigned long long value = 0;
                if (!FormatHelper::ParseUnsigned(argv[++i], value)) {
                    std::wcerr << L"ulimit: invalid " << opt_name << L" value: " << argv[i] << L"\n";
                    parse_error = true;
                    return false;
                }
                opt_val = value;
                return true;
            };

            if (arg == L"-c" || arg == L"--core") { if (!parse_opt(core_size_blocks, L"core size")) return; continue; }
            if (arg == L"-d" || arg == L"--data") { if (!parse_opt(data_seg_kb, L"data segment")) return; continue; }
            if (arg == L"-e" || arg == L"--nice") { if (!parse_opt(nice_priority, L"nice priority")) return; continue; }
            if (arg == L"-f" || arg == L"--file-size") { if (!parse_opt(file_size_blocks, L"file size")) return; continue; }
            if (arg == L"-i" || arg == L"--signals") { if (!parse_opt(pending_signals, L"pending signals")) return; continue; }
            if (arg == L"-l" || arg == L"--locked-mem") { if (!parse_opt(locked_mem_kb, L"locked memory")) return; continue; }
            if (arg == L"-m" || arg == L"--memory") { if (!parse_opt(memory_megabytes, L"memory")) return; continue; }
            if (arg == L"-n" || arg == L"--open-files") { if (!parse_opt(open_files, L"open files")) return; continue; }
            if (arg == L"-p" || arg == L"--pipe-size") { if (!parse_opt(pipe_size_blocks, L"pipe size")) return; continue; }
            if (arg == L"-q" || arg == L"--posix-mq") { if (!parse_opt(msg_queue_bytes, L"message queue")) return; continue; }
            if (arg == L"-r" || arg == L"--realtime-priority") { if (!parse_opt(rt_priority, L"real-time priority")) return; continue; }
            if (arg == L"-s" || arg == L"--stack") { if (!parse_opt(stack_size_kb, L"stack size")) return; continue; }
            if (arg == L"-t" || arg == L"--cpu") { if (!parse_opt(cpu_seconds, L"CPU time")) return; continue; }
            if (arg == L"-u" || arg == L"--processes") {
                unsigned long long value = 0;
                if (i + 1 < argc && (argv[i + 1][0] != L'-' || std::iswdigit(argv[i + 1][1]))) {
                    if (!FormatHelper::ParseUnsigned(argv[++i], value) || value > 0xFFFFFFFFULL) {
                        std::wcerr << L"ulimit: invalid process count: " << argv[i] << L"\n";
                        parse_error = true;
                        return;
                    }
                    process_count = static_cast<DWORD>(value);
                } else {
                    process_count = 0;
                }
                continue;
            }
            if (arg == L"-v" || arg == L"--virtual-memory") { if (!parse_opt(virtual_mem_mb, L"virtual memory")) return; continue; }
            if (arg == L"-w" || arg == L"--working-set") { if (!parse_opt(working_set_mb, L"working set")) return; continue; }
            if (arg == L"-x" || arg == L"--file-locks") { if (!parse_opt(file_locks, L"file locks")) return; continue; }

            if (arg == L"--") {
                for (++i; i < argc; ++i) command.push_back(argv[i] != nullptr ? argv[i] : L"");
                break;
            }

            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"ulimit: unknown option -- " << arg << L"\nTry 'ulimit --help' for more information.\n";
                parse_error = true;
                return;
            }

            for (; i < argc; ++i) command.push_back(argv[i] != nullptr ? argv[i] : L"");
            break;
        }
    }
