#include "csv_formatter.hpp"
#include "process_thread_info.hpp"

void CsvFormatter::format(std::ostream& os, const std::vector<ProcessThreadInfo>& data)  {
        os << "PID,ThreadCount,ProcessName\n";
        for (const auto& item : data) {
            os << item.pid << ","
               << item.threadCount << ",\""
               << escapeQuotes(item.narrowName()) << "\"\n";
        }
    }

std::string CsvFormatter::escapeQuotes(std::string str) {
        size_t pos = 0;
        while ((pos = str.find('\"', pos)) != std::string::npos) {
            str.replace(pos, 1, "\"\"");
            pos += 2;
        }
        return str;
    }
