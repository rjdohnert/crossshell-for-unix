#include "pipe_formatter.hpp"
#include "process_thread_info.hpp"

void PipeFormatter::format(std::ostream& os, const std::vector<ProcessThreadInfo>& data)  {
        os << "PID|ThreadCount|ProcessName\n";
        for (const auto& item : data) {
            os << item.pid << "|"
               << item.threadCount << "|"
               << item.narrowName() << "\n";
        }
    }
