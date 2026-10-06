#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "scm_handle.hpp"
#include "service_controller.hpp"
#include "stopsrc_app.hpp"
#include "stopsrc_options.hpp"

int StopsrcApplication::Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        StopsrcOptions opt;
        if (!m_parser.Parse(argc, argv, opt)) {
            std::cerr << "Try 'stopsrc --help' for usage.\n";
            return 2;
        }

        if (opt.help) {
            OptionParser::PrintHelp();
            return 0;
        }

        if (opt.version) {
            OptionParser::PrintVersion();
            return 0;
        }

        if (opt.service.empty()) {
            std::cerr << "stopsrc: missing required option -s SERVICE\n";
            return 2;
        }

        OutputFormatter formatter(opt.format, opt.pipe);

        ScmHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (!scm) {
            std::cerr << "stopsrc: cannot open Service Control Manager (error " << GetLastError() << ")\n";
            return 1;
        }

        ScmHandle svc(OpenServiceW(scm.Get(), opt.service.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS));
        if (!svc) {
            DWORD err = GetLastError();
            std::wcerr << L"stopsrc: cannot open service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        SERVICE_STATUS_PROCESS st{};
        DWORD needed = 0;
        if (QueryServiceStatusEx(svc.Get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&st), sizeof(st), &needed)) {
            if (st.dwCurrentState == SERVICE_STOPPED) {
                formatter.Emit(L"0513-044 The subsystem was requested to stop. Subsystem: " + opt.service);
                return 0;
            }
        }

        SERVICE_STATUS svcStatus{};
        if (!ControlService(svc.Get(), SERVICE_CONTROL_STOP, &svcStatus)) {
            DWORD err = GetLastError();
            std::wcerr << L"stopsrc: failed to stop service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        if (!ServiceController::WaitForState(svc.Get(), SERVICE_STOPPED, opt.timeoutMs)) {
            std::wcerr << L"stopsrc: timed out waiting for service to stop: " << opt.service << L"\n";
            return 1;
        }

        formatter.Emit(L"0513-044 The subsystem was requested to stop. Subsystem: " + opt.service);
        return 0;
    }
