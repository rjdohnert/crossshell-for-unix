#include "console_manager.hpp"
#include "nt_api_engine.hpp"
#include "output_formatter.hpp"
#include "pdh_rate_sample.hpp"
#include "shutdown_control.hpp"
#include "string_utils.hpp"
#include "system_sample.hpp"
#include "table_layout.hpp"
#include "vmstat_app.hpp"
#include "vmstat_options.hpp"

int VmstatApplication::Run(int argc, wchar_t* argv[]) {
        SetConsoleCtrlHandler(GlobalConsoleHandler, TRUE);

        bool ntApiInitialized = m_ntEngine.Init();
        bool pdhInitialized = m_pdhEngine.Init();

        auto finish = [&](int code) {
            m_pdhEngine.Shutdown();
            return code;
        };

        if (!ntApiInitialized) {
            std::cerr << "vmstat: NT performance counters unavailable; attempting PDH fallback.\n";
        }
        if (!pdhInitialized) {
            std::cerr << "vmstat: PDH performance counters unavailable; some rate fields may be N/A.\n";
        }

        VmstatOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return finish(1);
        }
        if (exitEarly) {
            return finish(0);
        }

        if (opts.summaryMode) {
            OutputFormatter::PrintSummary(m_ntEngine, m_pdhEngine);
            return finish(0);
        }

        SystemSample prevSample, currSample;
        if (!m_ntEngine.TakeSample(prevSample)) {
            std::cerr << "vmstat: failed to read system status.\n";
            return finish(1);
        }

        if (opts.interval <= 0) {
            opts.interval = 1;
            opts.maxCount = 1;
        }

        int printedLines = 0;
        int currentCount = 0;

        if (opts.outputMode == OutputMode::Table) {
            OutputFormatter::PrintHeader(opts.mode, opts.layoutMode);
            std::cerr << "vmstat: collecting baseline for " << opts.interval << " second(s)...\n";
        } else if (opts.outputMode == OutputMode::Csv) {
            std::cout << "sample,run,blk,thr,swpd,free,buff,cache,flt,pi,po,in,sy,cs,us,sy_cpu,id,src_flt,src_pi,src_po,src_in,src_sy,src_cs\n";
        }

        while (g_running) {
            {
                std::unique_lock<std::mutex> lock(g_shutdownMutex);
                if (g_shutdownCV.wait_for(lock, std::chrono::seconds(opts.interval), [] { return !g_running; })) {
                    break;
                }
            }
            if (!m_ntEngine.TakeSample(currSample)) break;

            double dt = std::chrono::duration<double>(currSample.timestamp - prevSample.timestamp).count();
            if (dt <= 0.001) dt = 1.0;

            ULONG activeProcs = currSample.psApiPerf.ProcessCount;
            ULONG totalThreads = currSample.psApiPerf.ThreadCount;
            ULONG blockedProcs = 0;

            uint64_t pageSize = currSample.psApiPerf.PageSize;
            uint64_t swpd  = currSample.psApiPerf.CommitTotal * pageSize;
            uint64_t freeM = currSample.psApiPerf.PhysicalAvailable * pageSize;
            uint64_t buff  = currSample.psApiPerf.KernelNonpaged * pageSize;
            uint64_t cache = (currSample.psApiPerf.KernelPaged + currSample.psApiPerf.SystemCache) * pageSize;

            const bool ntRateAvailable = prevSample.ntPerfValid && currSample.ntPerfValid;
            PdhRateSample pdhRates;
            (void)m_pdhEngine.CollectRates(pdhRates);

            double fltRate = 0.0;
            double piRate  = 0.0;
            double poRate  = 0.0;
            double inRate  = 0.0;
            double csRate  = 0.0;
            double syRate  = 0.0;
            bool fltAvailable = false;
            bool piAvailable = false;
            bool poAvailable = false;
            bool inAvailable = false;
            bool syAvailable = false;
            bool csAvailable = false;

            if (ntRateAvailable) {
                fltRate = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.PageFaultCount, prevSample.perfInfo.PageFaultCount, dt);
                piRate  = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.PageReadCount, prevSample.perfInfo.PageReadCount, dt);
                poRate  = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.PagefilePagesWritten, prevSample.perfInfo.PagefilePagesWritten, dt);
                csRate  = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.ContextSwitches, prevSample.perfInfo.ContextSwitches, dt);
                syRate  = NtApiEngine::ComputeDeltaRate(currSample.perfInfo.SystemCalls, prevSample.perfInfo.SystemCalls, dt);
                fltAvailable = true;
                piAvailable = true;
                poAvailable = true;
                syAvailable = true;
                csAvailable = true;
            }

            if (!fltAvailable && pdhRates.fltValid) {
                fltRate = NtApiEngine::ClampRate(pdhRates.fltPerSec);
                fltAvailable = true;
            }
            if (!piAvailable && pdhRates.pagesInValid) {
                piRate  = NtApiEngine::ClampRate(pdhRates.pagesInPerSec);
                piAvailable = true;
            }
            if (!poAvailable && pdhRates.pagesOutValid) {
                poRate  = NtApiEngine::ClampRate(pdhRates.pagesOutPerSec);
                poAvailable = true;
            }
            if (pdhRates.interruptsValid) {
                inRate  = NtApiEngine::ClampRate(pdhRates.interruptsPerSec);
                inAvailable = true;
            }
            if (!csAvailable && pdhRates.ctxSwitchValid) {
                csRate  = NtApiEngine::ClampRate(pdhRates.ctxSwitchPerSec);
                csAvailable = true;
            }
            if (!syAvailable && pdhRates.sysCallsValid) {
                syRate  = NtApiEngine::ClampRate(pdhRates.sysCallsPerSec);
                syAvailable = true;
            }

            const bool fltFromNt = ntRateAvailable;
            const bool piFromNt = ntRateAvailable;
            const bool poFromNt = ntRateAvailable;
            const bool syFromNt = ntRateAvailable;
            const bool csFromNt = ntRateAvailable;
            const char* srcFlt = NtApiEngine::SelectSourceTag(fltAvailable && fltFromNt, fltAvailable && !fltFromNt);
            const char* srcPi = NtApiEngine::SelectSourceTag(piAvailable && piFromNt, piAvailable && !piFromNt);
            const char* srcPo = NtApiEngine::SelectSourceTag(poAvailable && poFromNt, poAvailable && !poFromNt);
            const char* srcIn = NtApiEngine::SelectSourceTag(false, inAvailable);
            const char* srcSy = NtApiEngine::SelectSourceTag(syAvailable && syFromNt, syAvailable && !syFromNt);
            const char* srcCs = NtApiEngine::SelectSourceTag(csAvailable && csFromNt, csAvailable && !csFromNt);

            ULONGLONG idleDelta   = currSample.idleTime.QuadPart - prevSample.idleTime.QuadPart;
            ULONGLONG kernelDelta = currSample.kernelTime.QuadPart - prevSample.kernelTime.QuadPart;
            ULONGLONG userDelta   = currSample.userTime.QuadPart - prevSample.userTime.QuadPart;

            ULONGLONG sysWorkDelta = (kernelDelta >= idleDelta) ? (kernelDelta - idleDelta) : 0;
            ULONGLONG totalCpuDelta = sysWorkDelta + userDelta + idleDelta;

            int usPct = (totalCpuDelta > 0) ? static_cast<int>(std::round((double)userDelta / totalCpuDelta * 100.0)) : 0;
            int syPct = (totalCpuDelta > 0) ? static_cast<int>(std::round((double)sysWorkDelta / totalCpuDelta * 100.0)) : 0;
            int idPct = (totalCpuDelta > 0) ? static_cast<int>(std::round((double)idleDelta / totalCpuDelta * 100.0)) : 100;

            if (opts.outputMode == OutputMode::Table) {
                if (printedLines > 0 && printedLines % opts.headerInterval == 0) {
                    OutputFormatter::PrintHeader(opts.mode, opts.layoutMode);
                }

                TableLayout layout = ConsoleManager::BuildTableLayout(ConsoleManager::GetConsoleWidth(), opts.layoutMode);

                if (layout.splitRows) {
                    std::cout << std::right
                              << std::setw(layout.runWidth) << activeProcs << ' '
                              << std::setw(layout.blkWidth) << blockedProcs << ' '
                              << std::setw(layout.thrWidth) << totalThreads << ' '
                              << StringUtils::FormatValue(swpd, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(freeM, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(buff, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(cache, opts.mode, layout.memWidth)
                              << "\n"
                              << StringUtils::FormatLongOrNA(fltAvailable, static_cast<long>(fltRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(piAvailable, static_cast<long>(piRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(poAvailable, static_cast<long>(poRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(inAvailable, static_cast<long>(inRate), layout.inWidth) << ' '
                              << StringUtils::FormatLongOrNA(syAvailable, static_cast<long>(syRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(csAvailable, static_cast<long>(csRate), layout.rateWidth) << ' '
                              << std::setw(layout.cpuWidth) << usPct << ' '
                              << std::setw(layout.cpuWidth) << syPct << ' '
                              << std::setw(layout.cpuWidth) << idPct
                              << "\n"
                              << "src flt=" << srcFlt << " pi=" << srcPi << " po=" << srcPo
                              << " in=" << srcIn << " sy=" << srcSy << " cs=" << srcCs << "\n";
                    printedLines += 3;
                } else {
                    std::cout << std::right
                              << std::setw(layout.runWidth) << activeProcs << ' '
                              << std::setw(layout.blkWidth) << blockedProcs << ' '
                              << std::setw(layout.thrWidth) << totalThreads << ' '
                              << StringUtils::FormatValue(swpd, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(freeM, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(buff, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatValue(cache, opts.mode, layout.memWidth) << ' '
                              << StringUtils::FormatLongOrNA(fltAvailable, static_cast<long>(fltRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(piAvailable, static_cast<long>(piRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(poAvailable, static_cast<long>(poRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(inAvailable, static_cast<long>(inRate), layout.inWidth) << ' '
                              << StringUtils::FormatLongOrNA(syAvailable, static_cast<long>(syRate), layout.rateWidth) << ' '
                              << StringUtils::FormatLongOrNA(csAvailable, static_cast<long>(csRate), layout.rateWidth) << ' '
                              << std::setw(layout.cpuWidth) << usPct << ' '
                              << std::setw(layout.cpuWidth) << syPct << ' '
                              << std::setw(layout.cpuWidth) << idPct
                              << "\n"
                              << "src flt=" << srcFlt << " pi=" << srcPi << " po=" << srcPo
                              << " in=" << srcIn << " sy=" << srcSy << " cs=" << srcCs << "\n";
                    printedLines += 2;
                }
            } else if (opts.outputMode == OutputMode::Csv) {
                auto v = [](bool available, double value) -> std::string {
                    if (!available) return "N/A";
                    std::ostringstream ss;
                    ss << static_cast<long>(value);
                    return ss.str();
                };
                std::cout << (currentCount + 1) << ','
                          << activeProcs << ',' << blockedProcs << ',' << totalThreads << ','
                          << StringUtils::FormatValueRaw(swpd, opts.mode) << ',' << StringUtils::FormatValueRaw(freeM, opts.mode) << ','
                          << StringUtils::FormatValueRaw(buff, opts.mode) << ',' << StringUtils::FormatValueRaw(cache, opts.mode) << ','
                          << v(fltAvailable, fltRate) << ',' << v(piAvailable, piRate) << ','
                          << v(poAvailable, poRate) << ',' << v(inAvailable, inRate) << ','
                          << v(syAvailable, syRate) << ',' << v(csAvailable, csRate) << ','
                          << usPct << ',' << syPct << ',' << idPct << ','
                          << srcFlt << ',' << srcPi << ',' << srcPo << ',' << srcIn << ',' << srcSy << ',' << srcCs << '\n';
            } else {
                auto v = [](bool available, double value) -> std::string {
                    if (!available) return "null";
                    std::ostringstream ss;
                    ss << static_cast<long>(value);
                    return ss.str();
                };
                std::cout << "{\"sample\":" << (currentCount + 1)
                          << ",\"run\":" << activeProcs
                          << ",\"blk\":" << blockedProcs
                          << ",\"thr\":" << totalThreads
                          << ",\"swpd\":\"" << StringUtils::FormatValueRaw(swpd, opts.mode)
                          << "\",\"free\":\"" << StringUtils::FormatValueRaw(freeM, opts.mode)
                          << "\",\"buff\":\"" << StringUtils::FormatValueRaw(buff, opts.mode)
                          << "\",\"cache\":\"" << StringUtils::FormatValueRaw(cache, opts.mode)
                          << "\",\"flt\":" << v(fltAvailable, fltRate)
                          << ",\"pi\":" << v(piAvailable, piRate)
                          << ",\"po\":" << v(poAvailable, poRate)
                          << ",\"in\":" << v(inAvailable, inRate)
                          << ",\"sy\":" << v(syAvailable, syRate)
                          << ",\"cs\":" << v(csAvailable, csRate)
                          << ",\"us\":" << usPct
                          << ",\"sy_cpu\":" << syPct
                          << ",\"id\":" << idPct
                          << ",\"src_flt\":\"" << srcFlt
                          << "\",\"src_pi\":\"" << srcPi
                          << "\",\"src_po\":\"" << srcPo
                          << "\",\"src_in\":\"" << srcIn
                          << "\",\"src_sy\":\"" << srcSy
                          << "\",\"src_cs\":\"" << srcCs
                          << "\"}" << '\n';
            }

            currentCount++;
            prevSample = currSample;

            if (opts.maxCount > 0 && currentCount >= opts.maxCount) {
                break;
            }
        }

        return finish(0);
    }
