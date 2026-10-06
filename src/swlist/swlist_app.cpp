#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "software_collector.hpp"
#include "software_item.hpp"
#include "swlist_app.hpp"
#include "swlist_options.hpp"
#include "system_info.hpp"

int SwlistApplication::Run(int argc, wchar_t* argv[]) const {
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);

        SwlistOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            return 1;
        }

        if (opts.showHelp) {
            OptionParser::PrintHelp(argv[0]);
            return 0;
        }

        vector<SoftwareItem> allSoftware = SoftwareCollector::CollectInstalledSoftware();
        vector<SoftwareItem> filteredSoftware = SoftwareCollector::Filter(allSoftware, opts.searchPatterns, opts.showSystemComponents);

        wstring hostName = SystemInfo::GetHostNameString();
        OutputFormatter::EmitHeader(hostName);

        if (filteredSoftware.empty()) {
            wcout << L"# No software products matched the specified selection.\n";
            return 0;
        }

        if (opts.verbose) {
            OutputFormatter::EmitVerbose(filteredSoftware);
            return 0;
        }

        if (opts.attribute != L"ALL") {
            return OutputFormatter::EmitAttribute(filteredSoftware, opts.attribute);
        }

        if (opts.level == L"VENDOR") {
            OutputFormatter::EmitVendorGrouped(filteredSoftware);
            return 0;
        }

        OutputFormatter::EmitDefault(filteredSoftware);
        return 0;
    }
