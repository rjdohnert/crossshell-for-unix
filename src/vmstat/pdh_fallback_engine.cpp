#include "pdh_fallback_engine.hpp"
#include "pdh_fallback_state.hpp"
#include "pdh_rate_sample.hpp"

bool PdhFallbackEngine::AddPdhCounter(HQUERY query, const char* path, HCOUNTER& counter) {
        PDH_STATUS status = PdhAddEnglishCounterA(query, path, 0, &counter);
        if (status == ERROR_SUCCESS) return true;
        status = PdhAddCounterA(query, path, 0, &counter);
        return status == ERROR_SUCCESS;
    }

bool PdhFallbackEngine::ReadPdhCounterDouble(HCOUNTER counter, double& value) {
        PDH_FMT_COUNTERVALUE fmt{};
        PDH_STATUS status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &fmt);
        if (status != ERROR_SUCCESS) return false;
        if (fmt.CStatus != PDH_CSTATUS_VALID_DATA && fmt.CStatus != PDH_CSTATUS_NEW_DATA) return false;
        value = fmt.doubleValue;
        return true;
    }

bool PdhFallbackEngine::Init() {
        if (m_pdh.initialized) return true;
        if (PdhOpenQueryW(nullptr, 0, &m_pdh.query) != ERROR_SUCCESS) return false;

        bool ok = true;
        ok = ok && AddPdhCounter(m_pdh.query, "\\Memory\\Page Faults/sec", m_pdh.pageFaults);
        ok = ok && AddPdhCounter(m_pdh.query, "\\Memory\\Pages Input/sec", m_pdh.pagesIn);
        ok = ok && AddPdhCounter(m_pdh.query, "\\Memory\\Pages Output/sec", m_pdh.pagesOut);
        ok = ok && AddPdhCounter(m_pdh.query, "\\Processor(_Total)\\Interrupts/sec", m_pdh.interrupts);
        ok = ok && AddPdhCounter(m_pdh.query, "\\System\\System Calls/sec", m_pdh.systemCalls);
        ok = ok && AddPdhCounter(m_pdh.query, "\\System\\Context Switches/sec", m_pdh.contextSwitches);

        if (!ok || PdhCollectQueryData(m_pdh.query) != ERROR_SUCCESS) {
            PdhCloseQuery(m_pdh.query);
            m_pdh = PdhFallbackState{};
            return false;
        }

        m_pdh.initialized = true;
        return true;
    }

bool PdhFallbackEngine::CollectRates(PdhRateSample& sample) {
        sample = PdhRateSample{};
        if (!m_pdh.initialized) return false;
        if (PdhCollectQueryData(m_pdh.query) != ERROR_SUCCESS) return false;

        sample.fltValid = ReadPdhCounterDouble(m_pdh.pageFaults, sample.fltPerSec);
        sample.pagesInValid = ReadPdhCounterDouble(m_pdh.pagesIn, sample.pagesInPerSec);
        sample.pagesOutValid = ReadPdhCounterDouble(m_pdh.pagesOut, sample.pagesOutPerSec);
        sample.interruptsValid = ReadPdhCounterDouble(m_pdh.interrupts, sample.interruptsPerSec);
        sample.sysCallsValid = ReadPdhCounterDouble(m_pdh.systemCalls, sample.sysCallsPerSec);
        sample.ctxSwitchValid = ReadPdhCounterDouble(m_pdh.contextSwitches, sample.ctxSwitchPerSec);

        sample.anyValid = sample.fltValid || sample.pagesInValid || sample.pagesOutValid
                       || sample.interruptsValid || sample.sysCallsValid || sample.ctxSwitchValid;
        return sample.anyValid;
    }

void PdhFallbackEngine::Shutdown() {
        if (m_pdh.query != nullptr) {
            PdhCloseQuery(m_pdh.query);
        }
        m_pdh = PdhFallbackState{};
    }

bool PdhFallbackEngine::IsInitialized() const {
        return m_pdh.initialized;
    }
