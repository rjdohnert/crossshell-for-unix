#include "option_parser.hpp"
#include "scm_handle.hpp"
#include "service_controller.hpp"
#include "startsrc_app.hpp"
#include "startsrc_options.hpp"

int StartsrcApplication::Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        StartsrcOptions opt;
        if (!m_parser.Parse(argc, argv, opt)) {
            std::cerr << "Try 'startsrc --help' for usage.\n";
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
            std::cerr << "startsrc: missing required option -s SERVICE\n";
            return 2;
        }

        ScmHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (!scm) {
            std::cerr << "startsrc: cannot open Service Control Manager (error " << GetLastError() << ")\n";
            return 1;
        }

        ScmHandle svc(OpenServiceW(scm.Get(), opt.service.c_str(), SERVICE_START | SERVICE_QUERY_STATUS));
        if (!svc) {
            DWORD err = GetLastError();
            std::wcerr << L"startsrc: cannot open service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        SERVICE_STATUS_PROCESS st{};
        DWORD needed = 0;
        if (QueryServiceStatusEx(svc.Get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&st), sizeof(st), &needed)) {
            if (st.dwCurrentState == SERVICE_RUNNING) {
                std::wcout << L"0513-059 The subsystem has already been started. Subsystem: " << opt.service << L"\n";
                return 0;
            }
        }

        if (!StartServiceW(svc.Get(), 0, nullptr)) {
            DWORD err = GetLastError();
            std::wcerr << L"startsrc: failed to start service '" << opt.service << L"' (error " << err << L")\n";
            return 1;
        }

        if (!ServiceController::WaitForState(svc.Get(), SERVICE_RUNNING, opt.timeoutMs)) {
            std::wcerr << L"startsrc: timed out waiting for service to run: " << opt.service << L"\n";
            return 1;
        }

        std::wcout << L"0513-059 The subsystem has been started. Subsystem: " << opt.service << L"\n";
        return 0;
    }
