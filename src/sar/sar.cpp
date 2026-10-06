#include "sar.hpp"
#include "sar_options.hpp"
#include "system_sampler.hpp"
#include "report_formatter.hpp"

int main(int argc, char* argv[]) {
    SarOptions opts;
    if (!SarOptionParser::parse(argc, argv, opts)) {
        return 1;
    }

    PrintHeaderLine();
    if (!opts.all) {
        if (opts.csv || opts.json) {
            if (opts.cpu) PrintCsvSectionHeader("CPU", { "USR%", "SYS%", "WIO%", "IDLE%" });
            if (opts.memory) PrintCsvSectionHeader("MEMORY", { "FREEKB", "SWAPKB", "USED%", "SWP%", "TOTKB" });
            if (opts.disk || opts.io) PrintCsvSectionHeader("DISK", { "BUSY%", "AVGQ", "X/S", "B/S", "WAIT" });
            if (opts.diskDetail) PrintCsvSectionHeader("DISK DETAIL", { "DEV", "BUSY%", "AVGQ", "X/S", "B/S", "WAIT" });
            if (opts.page) PrintCsvSectionHeader("PAGE", { "PI/S", "PO/S" });
            if (opts.kernel) PrintCsvSectionHeader("KERNEL", { "PROC", "THREADS", "HANDLES" });
            if (opts.perCore) PrintCsvSectionHeader("CPU CORE", { "CORE", "USR%", "SYS%", "IDLE%" });
            if (opts.network) PrintCsvSectionHeader("NETWORK", { "RX/S", "TX/S", "RK/S", "TK/S" });
            if (opts.queue) PrintCsvSectionHeader("QUEUE", { "RUNQ", "OCC%", "PROC", "THR" });
        } else {
            if (opts.cpu) PrintSectionHeader("CPU", { "%USR", "%SYS", "%WIO", "%IDLE" });
            if (opts.memory) PrintSectionHeader("MEMORY", { "FREEKB", "SWAPKB", "%USED", "%SWP", "TOTKB" });
            if (opts.disk || opts.io) PrintSectionHeader("DISK", { "%BUSY", "AVGQ", "R+W/S", "BLKS/S", "AVWAIT" });
            if (opts.diskDetail) PrintSectionHeader("DISK DETAIL", { "DEV", "%BUSY", "AVGQ", "X/S", "B/S", "WAIT" });
            if (opts.page) PrintSectionHeader("PAGE", { "PI/S", "PO/S" });
            if (opts.kernel) PrintSectionHeader("KERNEL", { "PROC", "THREADS", "HANDLES" });
            if (opts.perCore) PrintSectionHeader("CPU CORE", { "CORE", "USR%", "SYS%", "IDLE%" });
            if (opts.network) PrintSectionHeader("NETWORK", { "RXPCK/S", "TXPCK/S", "RXKB/S", "TXKB/S" });
            if (opts.queue) PrintSectionHeader("QUEUE", { "RUNQ", "%RUNOCC", "PROCS", "THREADS" });
        }
    }

    SystemSampler sampler;

    // Warm-up delay for delta counters
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Summary accumulator arrays
    std::vector<CpuSample> cpuHistory;
    std::vector<MemorySample> memHistory;
    std::vector<DiskSample> diskHistory;
    std::vector<NetSample> netHistory;
    std::vector<QueueSample> queueHistory;
    std::vector<PagingSample> pageHistory;
    std::vector<KernelTableSample> kernelHistory;
    std::vector<CpuCoreSample> perCoreHistory;
    std::vector<std::string> cpuTimes;
    std::vector<std::string> memTimes;
    std::vector<std::string> diskTimes;
    std::vector<std::string> netTimes;
    std::vector<std::string> queueTimes;
    std::vector<std::string> pageTimes;
    std::vector<std::string> kernelTimes;
    std::vector<std::string> perCoreTimes;

    int currentSample = 0;
    while (opts.count == 0 || currentSample < opts.count) {
        std::string ts = GetCurrentTimestamp();

        // 1. CPU Report (-u)
        if (opts.cpu) {
            auto s = sampler.sampleCpu();
            cpuHistory.push_back(s);
            cpuTimes.push_back(ts);
            if (!opts.all) {
                if (opts.csv) {
                    std::cout << ts << "," << s.usr << "," << s.sys << "," << s.wio << "," << s.idle << "\n";
                } else if (opts.json) {
                    std::cout << "{\"time\":\"" << ts << "\",\"section\":\"CPU\",\"usr%\":" << s.usr << ",\"sys%\":" << s.sys << ",\"wio%\":" << s.wio << ",\"idle%\":" << s.idle << "}\n";
                } else {
                    std::cout << std::left << std::setw(8) << ts
                              << " usr=" << std::setw(7) << FormatPercent(s.usr)
                              << " sys=" << std::setw(7) << FormatPercent(s.sys)
                              << " wio=" << std::setw(7) << FormatPercent(s.wio)
                              << " idle=" << std::setw(7) << FormatPercent(s.idle) << "\n";
                }
            }
        }

        // 2. Memory Report (-r)
        if (opts.memory) {
            auto s = sampler.sampleMemory();
            memHistory.push_back(s);
            memTimes.push_back(ts);
            if (!opts.all) {
                if (opts.csv) {
                    std::cout << ts << "," << s.freeMemKb << "," << s.freeSwapKb << "," << s.memUsedPct << "," << s.swapUsedPct << "," << s.totalMemKb << "\n";
                } else if (opts.json) {
                    std::cout << "{\"time\":\"" << ts << "\",\"section\":\"MEMORY\",\"freeKB\":" << s.freeMemKb << ",\"swapKB\":" << s.freeSwapKb << ",\"used%\":" << s.memUsedPct << ",\"swp%\":" << s.swapUsedPct << ",\"totalKB\":" << s.totalMemKb << "}\n";
                } else {
                    std::cout << std::left << std::setw(8) << ts
                              << " free=" << std::setw(7) << FormatKib(s.freeMemKb)
                              << " swap=" << std::setw(7) << FormatKib(s.freeSwapKb)
                              << " used=" << std::setw(7) << FormatPercent(s.memUsedPct)
                              << " swp=" << std::setw(7) << FormatPercent(s.swapUsedPct)
                              << " total=" << std::setw(7) << FormatKib(s.totalMemKb) << "\n";
                }
            }
        }

        // 3. Disk / Block I/O Report (-d, -D, -b)
        if (opts.disk || opts.io) {
            auto s = sampler.sampleDisk();
            diskHistory.push_back(s);
            diskTimes.push_back(ts);
            if (!opts.all) {
                std::cout << std::left << std::setw(8) << ts
                          << " busy=" << std::setw(7) << FormatPercent(s.busyPct)
                          << " avgq=" << std::setw(6) << std::fixed << std::setprecision(1) << s.avgQueue
                          << " xfers=" << std::setw(7) << FormatRate(s.transfersPerSec, "/s")
                          << " blks=" << std::setw(7) << FormatRate(s.blocksPerSec, "/s")
                          << " wait=" << std::setw(7) << std::fixed << std::setprecision(1) << s.avgWaitMs << "ms\n";
            }
        }

        if (opts.diskDetail) {
            auto detail = sampler.sampleDiskDetail();
            if (!opts.all) {
                for (const auto& d : detail) {
                    std::cout << std::left << std::setw(8) << ts
                              << " dev=" << std::setw(6) << d.device
                              << " busy=" << std::setw(7) << FormatPercent(d.busyPct)
                              << " avgq=" << std::setw(6) << std::fixed << std::setprecision(1) << d.avgQueue
                              << " xfers=" << std::setw(7) << FormatRate(d.transfersPerSec, "/s")
                              << " blks=" << std::setw(7) << FormatRate(d.blocksPerSec, "/s")
                              << " wait=" << std::setw(7) << std::fixed << std::setprecision(1) << d.avgWaitMs << "ms\n";
                }
            }
        }

        if (opts.page) {
            auto s = sampler.samplePaging();
            pageHistory.push_back(s);
            pageTimes.push_back(ts);
            if (!opts.all) {
                if (opts.csv) {
                    std::cout << ts << "," << s.pagesInPerSec << "," << s.pagesOutPerSec << "\n";
                } else if (opts.json) {
                    std::cout << "{\"time\":\"" << ts << "\",\"section\":\"PAGE\",\"pi/s\":" << s.pagesInPerSec << ",\"po/s\":" << s.pagesOutPerSec << "}\n";
                } else {
                    std::cout << std::left << std::setw(8) << ts
                              << " pi/s=" << std::setw(7) << FormatRate(s.pagesInPerSec, "/s")
                              << " po/s=" << std::setw(7) << FormatRate(s.pagesOutPerSec, "/s") << "\n";
                }
            }
        }

        if (opts.kernel) {
            auto s = sampler.sampleKernelTable();
            kernelHistory.push_back(s);
            kernelTimes.push_back(ts);
            if (!opts.all) {
                if (opts.csv) {
                    std::cout << ts << "," << s.processCount << "," << s.threadCount << "," << s.handleCount << "\n";
                } else if (opts.json) {
                    std::cout << "{\"time\":\"" << ts << "\",\"section\":\"KERNEL\",\"proc\":" << s.processCount << ",\"threads\":" << s.threadCount << ",\"handles\":" << s.handleCount << "}\n";
                } else {
                    std::cout << std::left << std::setw(8) << ts
                              << " proc=" << std::setw(5) << s.processCount
                              << " threads=" << std::setw(5) << s.threadCount
                              << " handles=" << std::setw(6) << s.handleCount << "\n";
                }
            }
        }

        if (opts.perCore) {
            auto samples = sampler.sampleCpuCores();
            for (const auto& s : samples) {
                perCoreHistory.push_back(s);
                perCoreTimes.push_back(ts);
                if (!opts.all) {
                    if (opts.csv) {
                        std::cout << ts << "," << s.core << "," << s.usr << "," << s.sys << "," << s.idle << "\n";
                    } else if (opts.json) {
                        std::cout << "{\"TIME\":\"" << ts << "\",\"SECTION\":\"CPU CORE\",\"CORE\":" << s.core << ",\"USR%\":" << s.usr << ",\"SYS%\":" << s.sys << ",\"IDLE%\":" << s.idle << "}\n";
                    } else {
                        std::cout << std::left << std::setw(8) << ts
                                  << " core=" << std::setw(3) << s.core
                                  << " usr=" << std::setw(7) << FormatPercent(s.usr)
                                  << " sys=" << std::setw(7) << FormatPercent(s.sys)
                                  << " idle=" << std::setw(7) << FormatPercent(s.idle) << "\n";
                    }
                }
            }
        }

        // 4. Network Report (-n)
        if (opts.network) {
            auto s = sampler.sampleNetwork(opts.interval);
            netHistory.push_back(s);
            netTimes.push_back(ts);
            if (!opts.all) {
                std::cout << std::left << std::setw(8) << ts
                          << " rx=" << std::setw(7) << FormatRate(s.rxPckSec, "/s")
                          << " tx=" << std::setw(7) << FormatRate(s.txPckSec, "/s")
                          << " rKB=" << std::setw(7) << FormatRate(s.rxKbSec, "K/s")
                          << " tKB=" << std::setw(7) << FormatRate(s.txKbSec, "K/s") << "\n";
            }
        }

        // 5. Queue Report (-q)
        if (opts.queue) {
            auto s = sampler.sampleQueue();
            queueHistory.push_back(s);
            queueTimes.push_back(ts);
            if (!opts.all) {
                std::cout << std::left << std::setw(8) << ts
                          << " runq=" << std::setw(5) << std::fixed << std::setprecision(1) << s.runqSz
                          << " occ=" << std::setw(6) << FormatPercent(s.runOccPct)
                          << " procs=" << std::setw(5) << s.processCount
                          << " threads=" << std::setw(5) << s.threadCount << "\n";
            }
        }

        currentSample++;
        if (opts.count == 0 || currentSample < opts.count) {
            std::this_thread::sleep_for(std::chrono::seconds(opts.interval));
        }
    }

    if (opts.all) {
        if (opts.cpu) {
            PrintSectionHeader("CPU", { "USR%", "SYS%", "WIO%", "IDLE%" });
            for (size_t i = 0; i < cpuHistory.size(); ++i) {
                const auto& s = cpuHistory[i];
                std::cout << std::left << std::setw(8) << cpuTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatPercent(s.usr)
                          << " " << std::setw(12) << FormatPercent(s.sys)
                          << " " << std::setw(12) << FormatPercent(s.wio)
                          << " " << std::setw(12) << FormatPercent(s.idle) << "\n";
            }
            std::cout << "\n";
        }
        if (opts.memory) {
            PrintSectionHeader("MEMORY", { "FREE", "SWAP", "USED%", "SWP%", "TOTAL" });
            for (size_t i = 0; i < memHistory.size(); ++i) {
                const auto& s = memHistory[i];
                std::cout << std::left << std::setw(8) << memTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatKib(s.freeMemKb)
                          << " " << std::setw(12) << FormatKib(s.freeSwapKb)
                          << " " << std::setw(12) << FormatPercent(s.memUsedPct)
                          << " " << std::setw(12) << FormatPercent(s.swapUsedPct)
                          << " " << std::setw(12) << FormatKib(s.totalMemKb) << "\n";
            }
            std::cout << "\n";
        }
        if (opts.disk || opts.io) {
            PrintSectionHeader("DISK", { "BUSY%", "AVGQ", "X/S", "B/S", "WAIT" });
            for (size_t i = 0; i < diskHistory.size(); ++i) {
                const auto& s = diskHistory[i];
                std::cout << std::left << std::setw(8) << diskTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatPercent(s.busyPct)
                          << " " << std::setw(12) << s.avgQueue
                          << " " << std::setw(12) << FormatRate(s.transfersPerSec, "/s")
                          << " " << std::setw(12) << FormatRate(s.blocksPerSec, "/s")
                          << " " << std::setw(12) << FormatWaitMs(s.avgWaitMs) << "\n";
            }
            std::cout << "\n";
        }
        if (opts.diskDetail) {
            PrintSectionHeader("DISK DETAIL", { "DEVICE", "BUSY%", "AVGQ", "X/S", "B/S", "WAIT" });
            std::vector<DiskSample> detail;
            for (size_t i = 0; i < diskTimes.size() && i < 1; ++i) {
                detail = sampler.sampleDiskDetail();
                for (const auto& d : detail) {
                    std::cout << std::left << std::setw(8) << diskTimes[i]
                              << " " << std::left << std::fixed << std::setprecision(1)
                              << std::setw(12) << d.device
                              << " " << std::setw(12) << FormatPercent(d.busyPct)
                              << " " << std::setw(12) << d.avgQueue
                              << " " << std::setw(12) << FormatRate(d.transfersPerSec, "/s")
                              << " " << std::setw(12) << FormatRate(d.blocksPerSec, "/s")
                              << " " << std::setw(12) << FormatWaitMs(d.avgWaitMs) << "\n";
                }
            }
            std::cout << "\n";
        }
        if (opts.page) {
            PrintSectionHeader("PAGE", { "PI/S", "PO/S" });
            for (size_t i = 0; i < pageHistory.size(); ++i) {
                const auto& s = pageHistory[i];
                std::cout << std::left << std::setw(8) << pageTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatRate(s.pagesInPerSec, "/s")
                          << " " << std::setw(12) << FormatRate(s.pagesOutPerSec, "/s") << "\n";
            }
            std::cout << "\n";
        }
        if (opts.kernel) {
            PrintSectionHeader("KERNEL", { "PROC", "THREADS", "HANDLES" });
            for (size_t i = 0; i < kernelHistory.size(); ++i) {
                const auto& s = kernelHistory[i];
                std::cout << std::left << std::setw(8) << kernelTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << s.processCount
                          << " " << std::setw(12) << s.threadCount
                          << " " << std::setw(12) << s.handleCount << "\n";
            }
            std::cout << "\n";
        }
        if (opts.perCore) {
            PrintSectionHeader("CPU CORE", { "CORE", "USR%", "SYS%", "IDLE%" });
            for (size_t i = 0; i < perCoreHistory.size(); ++i) {
                const auto& s = perCoreHistory[i];
                std::cout << std::left << std::setw(8) << perCoreTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << s.core
                          << " " << std::setw(12) << FormatPercent(s.usr)
                          << " " << std::setw(12) << FormatPercent(s.sys)
                          << " " << std::setw(12) << FormatPercent(s.idle) << "\n";
            }
            std::cout << "\n";
        }
        if (opts.network) {
            PrintSectionHeader("NETWORK", { "RX/S", "TX/S", "RK/S", "TK/S" });
            for (size_t i = 0; i < netHistory.size(); ++i) {
                const auto& s = netHistory[i];
                std::cout << std::left << std::setw(8) << netTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << FormatRate(s.rxPckSec, "/s")
                          << " " << std::setw(12) << FormatRate(s.txPckSec, "/s")
                          << " " << std::setw(12) << FormatRate(s.rxKbSec, "K/s")
                          << " " << std::setw(12) << FormatRate(s.txKbSec, "K/s") << "\n";
            }
            std::cout << "\n";
        }
        if (opts.queue) {
            PrintSectionHeader("QUEUE", { "RUNQ", "OCC%", "PROC", "THR" });
            for (size_t i = 0; i < queueHistory.size(); ++i) {
                const auto& s = queueHistory[i];
                std::cout << std::left << std::setw(8) << queueTimes[i]
                          << " " << std::left << std::fixed << std::setprecision(1)
                          << std::setw(12) << s.runqSz
                          << " " << std::setw(12) << FormatPercent(s.runOccPct)
                          << " " << std::setw(12) << s.processCount
                          << " " << std::setw(12) << s.threadCount << "\n";
            }
            std::cout << "\n";
        }
    }

    // Print Averages Line (HP-UX Standard Behavior)
    if (currentSample > 1) {
        std::cout << "\n";
        if (opts.cpu) {
            double u = 0, sys = 0, w = 0, id = 0;
            for (const auto& c : cpuHistory) { u += c.usr; sys += c.sys; w += c.wio; id += c.idle; }
            size_t n = cpuHistory.size();
            std::cout << std::left << std::setw(10) << "Average"
                      << std::right << std::fixed << std::setprecision(1)
                      << std::setw(8) << (u / n) << std::setw(8) << (sys / n)
                      << std::setw(8) << (w / n) << std::setw(8) << (id / n) << "  (CPU %USR %SYS %WIO %IDLE)\n";
        }
    }

    return 0;
}