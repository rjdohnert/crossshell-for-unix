#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "privilege_manager.hpp"
#include "process_suspend_engine.hpp"
#include "suspend_app.hpp"
#include "suspend_options.hpp"

int SuspendApplication::Run(int argc, wchar_t* argv[]) const {
        if (argc > 0 && argv[0] != nullptr && (argc == 1 || std::wstring(argv[0]).empty())) {
            OptionParser::PrintUsage(L"suspend");
            return 1;
        }

        SuspendOptions options = m_parser.Parse(argc, argv);

        if (options.show_version) {
            OptionParser::PrintVersion();
            return 0;
        }

        if (options.show_help) {
            const wchar_t* program_name = (argc > 0 && argv[0] != nullptr) ? argv[0] : L"suspend";
            OptionParser::PrintUsage(program_name);
            return 0;
        }

        if (options.parse_error) {
            return 1;
        }

        if (options.pids.empty()) {
            OptionParser::PrintUsage((argc > 0 && argv[0] != nullptr) ? argv[0] : L"suspend");
            return 1;
        }

        if (options.enable_debug_priv && !PrivilegeManager::EnableDebugPrivilege()) {
            std::wcerr << L"suspend: warning: failed to enable SeDebugPrivilege; continuing without it\n";
        }

        bool all_ok = true;
        for (DWORD pid : options.pids) {
            if (!ProcessSuspendEngine::OperateOnPid(pid, options.resume, options.use_ntdll)) {
                std::wcerr << L"suspend: failed to " << (options.resume ? L"resume" : L"suspend") << L" process " << pid << L"\n";
                all_ok = false;
            }
        }

        OutputFormatter::Emit(options.output_format, options.pipe_command, options.resume, all_ok);
        return all_ok ? 0 : 1;
    }
