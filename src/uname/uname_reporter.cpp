#include "system_details.hpp"
#include "uname_options.hpp"
#include "uname_reporter.hpp"

std::string UnameReporter::toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size, NULL, NULL);
        return result;
    }

int UnameReporter::dispatch(const SystemDetails& sys, const UnameOptions& opts) {
        std::wstring text;

        if (opts.printExtended) {
            std::wstringstream ss;
            ss << L"System = " << sys.sysname << L"\n"
               << L"Node = " << sys.nodename << L"\n"
               << L"Release = " << sys.release << L"\n"
               << L"KernelID = " << sys.version << L"\n"
               << L"Machine = " << sys.machine << L"\n"
               << L"Edition = " << sys.marketingEdition << L"\n"
               << L"Model = " << sys.model << L"\n"
               << L"NumCPU = " << sys.numProcessors << L"\n"
               << L"HardwareID = " << (sys.machineId.empty() ? L"N/A" : sys.machineId) << L"\n";
            text = ss.str();
        } else if (opts.outputFormat == 1) {
            text = L"{\"sysname\":\"" + sys.sysname + L"\",\"nodename\":\"" + sys.nodename +
                   L"\",\"release\":\"" + sys.release + L"\",\"version\":\"" + sys.version +
                   L"\",\"machine\":\"" + sys.machine + L"\"}\n";
        } else if (opts.outputFormat == 2) {
            text = L"sysname,nodename,release,version,machine\n\"" +
                   sys.sysname + L"\",\"" + sys.nodename + L"\",\"" + sys.release +
                   L"\",\"" + sys.version + L"\",\"" + sys.machine + L"\"\n";
        } else if (opts.outputFormat == 3) {
            text = L"SYSNAME\tNODENAME\tRELEASE\tVERSION\tMACHINE\n" +
                   sys.sysname + L"\t" + sys.nodename + L"\t" + sys.release +
                   L"\t" + sys.version + L"\t" + sys.machine + L"\n";
        } else {
            std::vector<std::wstring> parts;
            if (opts.printSysname) parts.push_back(sys.sysname);
            if (opts.printNodename) parts.push_back(sys.nodename);
            if (opts.printRelease) parts.push_back(sys.release);
            if (opts.printVersion) parts.push_back(sys.version);
            if (opts.printMachine) parts.push_back(sys.machine);
            if (opts.printId && !sys.machineId.empty()) parts.push_back(sys.machineId);
            if (opts.printLicense && !sys.licenseId.empty()) parts.push_back(sys.licenseId);
            if (opts.printModel && !sys.model.empty()) parts.push_back(sys.model);

            for (size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) text += L" ";
                text += parts[i];
            }
            text += L"\n";
        }

        if (!opts.pipeCommand.empty()) {
            FILE* pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
            if (!pipe) return 1;
            std::string utf8 = toUtf8(text);
            std::fwrite(utf8.data(), 1, utf8.size(), pipe);
            _pclose(pipe);
        } else {
            std::wcout << text;
        }
        return 0;
    }
