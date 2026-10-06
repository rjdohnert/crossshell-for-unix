#include "process_thread_info.hpp"
#include "table_formatter.hpp"

void TableFormatter::format(std::ostream& os, const std::vector<ProcessThreadInfo>& data)  {
        size_t maxNameLen = 12; // Base length for "ProcessName"
        for (const auto& item : data) {
            maxNameLen = (std::max)(maxNameLen, item.narrowName().length());
        }

        // Table Header
        os << std::left 
           << std::setw(10) << "PID"
           << std::setw(14) << "Threads"
           << std::setw(maxNameLen) << "ProcessName" 
           << "\n";

        os << std::string(10 + 14 + maxNameLen, '-') << "\n";

        // Rows
        for (const auto& item : data) {
            os << std::left 
               << std::setw(10) << item.pid
               << std::setw(14) << item.threadCount
               << item.narrowName() << "\n";
        }
    }
