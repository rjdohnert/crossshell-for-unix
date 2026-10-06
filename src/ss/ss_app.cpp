#include "socket_row.hpp"
#include "socket_statistics_collector.hpp"
#include "ss_app.hpp"
#include "ss_options.hpp"
#include "ss_reporter.hpp"
#include "winsock_scope.hpp"

int SsApplication::Run(int argc, wchar_t* argv[]) {
        WinsockScope winsock;
        if (!winsock.IsInitialized()) {
            std::wcerr << L"ss: failed to initialize Winsock\n";
            return 1;
        }

        SsOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"ss");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage(argc > 0 ? argv[0] : L"ss");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        std::vector<SocketRow> rows;
        bool ok = true;
        if (opts.showTcp) ok = SocketStatisticsCollector::CollectTcpRows(rows, opts.listeningOnly) && ok;
        if (opts.showUdp) ok = SocketStatisticsCollector::CollectUdpRows(rows, opts.listeningOnly) && ok;

        std::sort(rows.begin(), rows.end(), [](const SocketRow& a, const SocketRow& b) {
            if (a.proto != b.proto) return a.proto < b.proto;
            if (a.local != b.local) return a.local < b.local;
            return a.pid < b.pid;
        });

        SsReporter::Emit(rows);

        if (!ok) {
            std::wcerr << L"ss: warning: some socket tables could not be read\n";
        }

        return 0;
    }
