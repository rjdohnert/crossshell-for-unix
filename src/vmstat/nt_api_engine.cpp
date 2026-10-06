#include "nt_api_engine.hpp"
#include "system_sample.hpp"

bool NtApiEngine::Init() {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return false;
        m_ntQuery = reinterpret_cast<pfnNtQuerySystemInformation>(GetProcAddress(hNtdll, "NtQuerySystemInformation"));
        return m_ntQuery != nullptr;
    }

bool NtApiEngine::TakeSample(SystemSample& sample) {
        sample.timestamp = std::chrono::steady_clock::now();
        sample.ntPerfValid = false;
        ZeroMemory(&sample.perfInfo, sizeof(sample.perfInfo));

        if (m_ntQuery) {
            NTSTATUS status = m_ntQuery(2, &sample.perfInfo, sizeof(sample.perfInfo), nullptr);
            sample.ntPerfValid = (status == STATUS_SUCCESS);
        }

        sample.psApiPerf.cb = sizeof(PERFORMANCE_INFORMATION);
        if (!GetPerformanceInfo(&sample.psApiPerf, sizeof(PERFORMANCE_INFORMATION))) {
            return false;
        }

        FILETIME ftIdle, ftKernel, ftUser;
        if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
            return false;
        }

        sample.idleTime.LowPart   = ftIdle.dwLowDateTime;
        sample.idleTime.HighPart  = ftIdle.dwHighDateTime;
        sample.kernelTime.LowPart = ftKernel.dwLowDateTime;
        sample.kernelTime.HighPart= ftKernel.dwHighDateTime;
        sample.userTime.LowPart   = ftUser.dwLowDateTime;
        sample.userTime.HighPart  = ftUser.dwHighDateTime;

        return true;
    }

double NtApiEngine::ClampRate(double value) {
        if (value < 0.0) return 0.0;
        if (value > 1000000000.0) return 1000000000.0;
        return value;
    }

double NtApiEngine::ComputeDeltaRate(ULONGLONG curr, ULONGLONG prev, double dt) {
        if (dt <= 0.0 || curr < prev) return 0.0;
        return ClampRate(static_cast<double>(curr - prev) / dt);
    }

const char* NtApiEngine::SelectSourceTag(bool ntAvailable, bool pdhAvailable) {
        if (ntAvailable) return "NT";
        if (pdhAvailable) return "PDH";
        return "N/A";
    }
