#include "process_thread_info.hpp"
#include "scoped_handle.hpp"
#include "thread_collector.hpp"

std::vector<ProcessThreadInfo> ThreadCollector::collect() {
        std::vector<ProcessThreadInfo> results;

        ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (!snapshot.isValid()) {
            return results;
        }

        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (!Process32FirstW(snapshot.get(), &pe32)) {
            return results;
        }

        do {
            ProcessThreadInfo info;
            info.pid = pe32.th32ProcessID;
            info.threadCount = pe32.cntThreads; // Populated by OS snapshot
            info.name = pe32.szExeFile;

            results.push_back(info);
        } while (Process32NextW(snapshot.get(), &pe32));

        return results;
    }
