#include "console_manager.hpp"
#include "nt_api_engine.hpp"
#include "output_formatter.hpp"
#include "pdh_fallback_engine.hpp"
#include "pdh_rate_sample.hpp"
#include "system_sample.hpp"
#include "table_layout.hpp"

void OutputFormatter::PrintHeader(UnitMode mode, LayoutMode layoutMode) {
        std::string unitStr = (mode == UnitMode::Megabytes) ? "(MB)" : (mode == UnitMode::Human ? "(Auto)" : "(KB)");
        int consoleWidth = ConsoleManager::GetConsoleWidth();
        TableLayout layout = ConsoleManager::BuildTableLayout(consoleWidth, layoutMode);

        if (layout.compactBanner) {
            std::cout << "procs | memory " << unitStr << " | paging(/s) | faults(/s) | cpu(%)\n";
        } else {
            std::cout << "----procs---- "
                      << "------------------------memory " << std::left << std::setw(7) << unitStr << std::right << "------------------------ "
                      << "------paging (/s)------ "
                      << "------faults (/s)------ "
                      << "---cpu (%)---\n";
        }

        if (layout.splitRows) {
            std::cout << std::right
                      << std::setw(layout.runWidth) << "run" << ' '
                      << std::setw(layout.blkWidth) << "blk" << ' '
                      << std::setw(layout.thrWidth) << "thr" << ' '
                      << std::setw(layout.memWidth) << "swpd" << ' '
                      << std::setw(layout.memWidth) << "free" << ' '
                      << std::setw(layout.memWidth) << "buff" << ' '
                      << std::setw(layout.memWidth) << "cache" << "\n"
                      << std::setw(layout.rateWidth) << "flt" << ' '
                      << std::setw(layout.rateWidth) << "pi" << ' '
                      << std::setw(layout.rateWidth) << "po" << ' '
                      << std::setw(layout.inWidth) << "in" << ' '
                      << std::setw(layout.rateWidth) << "sy" << ' '
                      << std::setw(layout.rateWidth) << "cs" << ' '
                      << std::setw(layout.cpuWidth) << "us" << ' '
                      << std::setw(layout.cpuWidth) << "sy" << ' '
                      << std::setw(layout.cpuWidth) << "id"
                      << "\n";
        } else {
            std::cout << std::right
                      << std::setw(layout.runWidth) << "run" << ' '
                      << std::setw(layout.blkWidth) << "blk" << ' '
                      << std::setw(layout.thrWidth) << "thr" << ' '
                      << std::setw(layout.memWidth) << "swpd" << ' '
                      << std::setw(layout.memWidth) << "free" << ' '
                      << std::setw(layout.memWidth) << "buff" << ' '
                      << std::setw(layout.memWidth) << "cache" << ' '
                      << std::setw(layout.rateWidth) << "flt" << ' '
                      << std::setw(layout.rateWidth) << "pi" << ' '
                      << std::setw(layout.rateWidth) << "po" << ' '
                      << std::setw(layout.inWidth) << "in" << ' '
                      << std::setw(layout.rateWidth) << "sy" << ' '
                      << std::setw(layout.rateWidth) << "cs" << ' '
                      << std::setw(layout.cpuWidth) << "us" << ' '
                      << std::setw(layout.cpuWidth) << "sy" << ' '
                      << std::setw(layout.cpuWidth) << "id"
                      << "\n";
        }
    }

void OutputFormatter::PrintSummary(NtApiEngine& ntEngine, PdhFallbackEngine& pdhEngine) {
        SystemSample sample;
        if (!ntEngine.TakeSample(sample)) {
            std::cerr << "vmstat: failed to gather system performance counters.\n";
            return;
        }

        uint64_t pageSize = sample.psApiPerf.PageSize;
        uint64_t totalPhys = sample.psApiPerf.PhysicalTotal * pageSize;
        uint64_t freePhys  = sample.psApiPerf.PhysicalAvailable * pageSize;
        uint64_t commitTot = sample.psApiPerf.CommitTotal * pageSize;
        uint64_t commitLim = sample.psApiPerf.CommitLimit * pageSize;

        std::cout << std::setw(14) << sample.psApiPerf.ProcessCount  << " processes\n"
                  << std::setw(14) << sample.psApiPerf.ThreadCount   << " threads\n"
                  << std::setw(14) << (totalPhys / 1024)             << " KB total physical memory\n"
                  << std::setw(14) << (freePhys / 1024)              << " KB available physical memory\n"
                  << std::setw(14) << (commitTot / 1024)             << " KB committed virtual memory\n"
                  << std::setw(14) << (commitLim / 1024)             << " KB total commit limit\n";

        if (sample.ntPerfValid) {
            std::cout << std::setw(14) << sample.perfInfo.PageFaultCount       << " page faults\n"
                      << std::setw(14) << sample.perfInfo.PageReadCount        << " page read operations\n"
                      << std::setw(14) << sample.perfInfo.PagefilePagesWritten << " pages written to pagefile\n"
                      << std::setw(14) << sample.perfInfo.ContextSwitches      << " context switches\n"
                      << std::setw(14) << sample.perfInfo.SystemCalls          << " system calls\n";
        } else {
            PdhRateSample pdhRates;
            bool pdhAvailable = pdhEngine.CollectRates(pdhRates) && pdhRates.anyValid;
            if (pdhAvailable) {
                std::cout << std::setw(14) << (pdhRates.fltValid ? std::to_string(static_cast<long>(pdhRates.fltPerSec)) : std::string("N/A")) << " page faults/sec (PDH)\n"
                          << std::setw(14) << (pdhRates.pagesInValid ? std::to_string(static_cast<long>(pdhRates.pagesInPerSec)) : std::string("N/A")) << " pages input/sec (PDH)\n"
                          << std::setw(14) << (pdhRates.pagesOutValid ? std::to_string(static_cast<long>(pdhRates.pagesOutPerSec)) : std::string("N/A")) << " pages output/sec (PDH)\n"
                          << std::setw(14) << (pdhRates.ctxSwitchValid ? std::to_string(static_cast<long>(pdhRates.ctxSwitchPerSec)) : std::string("N/A")) << " context switches/sec (PDH)\n"
                          << std::setw(14) << (pdhRates.sysCallsValid ? std::to_string(static_cast<long>(pdhRates.sysCallsPerSec)) : std::string("N/A")) << " system calls/sec (PDH)\n";
            } else {
                std::cout << std::setw(14) << "N/A" << " page faults (NT/PDH counters unavailable)\n"
                          << std::setw(14) << "N/A" << " page read operations (NT/PDH counters unavailable)\n"
                          << std::setw(14) << "N/A" << " pages written to pagefile (NT/PDH counters unavailable)\n"
                          << std::setw(14) << "N/A" << " context switches (NT/PDH counters unavailable)\n"
                          << std::setw(14) << "N/A" << " system calls (NT/PDH counters unavailable)\n";
            }
        }
    }
