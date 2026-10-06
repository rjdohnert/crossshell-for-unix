#include "socket_row.hpp"
#include "ss_reporter.hpp"

void SsReporter::Emit(const std::vector<SocketRow>& rows) {
        std::wcout << std::left
                   << std::setw(7) << L"Netid"
                   << std::setw(34) << L"Local Address:Port"
                   << std::setw(34) << L"Peer Address:Port"
                   << std::setw(13) << L"State"
                   << L"PID\n";

        for (const auto& row : rows) {
            std::wcout << std::left
                       << std::setw(7) << row.proto
                       << std::setw(34) << row.local
                       << std::setw(34) << row.peer
                       << std::setw(13) << row.state
                       << row.pid << L"\n";
        }
    }
