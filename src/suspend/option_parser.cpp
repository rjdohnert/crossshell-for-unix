#include "option_parser.hpp"
#include "string_utils.hpp"
#include "suspend_options.hpp"

void OptionParser::PrintUsage(const wchar_t* program_name) {
        std::wcout << L"Usage: " << program_name << L" [options] PID...\n"
                   << L"Suspend or resume one or more processes.\n\n"
                   << L"Options:\n"
                   << L"  -r, --resume   Resume the target processes instead of suspending them\n"
                   << L"      --safe     Force conservative mode (no debug privilege, no ntdll fast path)\n"
                   << L"  -d, --debug    Enable SeDebugPrivilege (off by default)\n"
                   << L"      --no-ntdll Avoid NtSuspendProcess/NtResumeProcess fast path\n"
                   << L"  -h, --help     Display this help text\n"
                   << L"      --json     Output operation status as JSON\n"
                   << L"      --csv      Output operation status as CSV\n"
                   << L"      --table    Output operation status as a table\n"
                   << L"      --pipe COMMAND  Send output through COMMAND\n"
                   << L"      --version  Display version information\n\n"
                   << L"Examples:\n"
                   << L"  suspend 1234\n"
                   << L"  suspend -r 1234\n"
                   << L"  suspend --debug 1234\n"
                   << L"  suspend --safe 1234\n";
    }

void OptionParser::PrintVersion() {
        std::wcout << L"suspend v1.0.0\n";
    }

SuspendOptions OptionParser::Parse(int argc, wchar_t* argv[]) const {
        SuspendOptions options;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] != nullptr ? argv[i] : L"";

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                options.show_help = true;
                continue;
            }

            if (arg == L"--version") {
                options.show_version = true;
                continue;
            }

            if (arg == L"-r" || arg == L"--resume") {
                options.resume = true;
                continue;
            }

            if (arg == L"--safe") {
                options.safe_mode = true;
                continue;
            }
            if (arg == L"--json") { options.output_format = 1; continue; }
            if (arg == L"--csv") { options.output_format = 2; continue; }
            if (arg == L"--table") { options.output_format = 3; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { options.pipe_command = argv[++i]; continue; }

            if (arg == L"-d" || arg == L"--debug") {
                options.enable_debug_priv = true;
                continue;
            }

            if (arg == L"--no-ntdll") {
                options.use_ntdll = false;
                continue;
            }

            if (arg == L"--") {
                for (++i; i < argc; ++i) {
                    DWORD pid = 0;
                    if (!StringUtils::ParsePid(argv[i], pid)) {
                        std::wcerr << L"suspend: illegal process id: " << argv[i] << L"\n";
                        options.parse_error = true;
                        return options;
                    }
                    options.pids.push_back(pid);
                }
                break;
            }

            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"suspend: unknown option -- " << arg << L"\n";
                options.parse_error = true;
                return options;
            }

            DWORD pid = 0;
            if (!StringUtils::ParsePid(arg, pid)) {
                std::wcerr << L"suspend: illegal process id: " << arg << L"\n";
                options.parse_error = true;
                return options;
            }
            options.pids.push_back(pid);
        }

        if (options.safe_mode) {
            options.enable_debug_priv = false;
            options.use_ntdll = false;
        }

        return options;
    }
