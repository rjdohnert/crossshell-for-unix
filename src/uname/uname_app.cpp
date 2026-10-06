#include "hostname_configurator.hpp"
#include "nt_kernel_info_provider.hpp"
#include "system_details.hpp"
#include "uname_app.hpp"
#include "uname_options.hpp"
#include "uname_reporter.hpp"

int UnameApp::run(int argc, wchar_t* argv[]) {
        UnameOptions options;
        if (!UnameOptions::parse(argc, argv, options)) {
            return 1;
        }

        if (!options.setHostname.empty()) {
            if (HostnameConfigurator::setHostname(options.setHostname)) {
                std::wcout << L"uname: hostname successfully updated to '" << options.setHostname << L"'\n";
                return 0;
            } else {
                std::wcerr << L"uname: error setting hostname (requires Administrator elevation, Error: " << GetLastError() << L")\n";
                return 1;
            }
        }

        SystemDetails sys = NtKernelInfoProvider::collect();
        return UnameReporter::dispatch(sys, options);
    }
