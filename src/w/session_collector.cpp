#include "process_info.hpp"
#include "session_collector.hpp"
#include "session_data.hpp"
#include "string_encoding.hpp"
#include "wts_deleter.hpp"

std::vector<SessionData> collectUserSessions(PWTS_SESSION_INFOW pSessions, DWORD sessionCount, const std::wstring& targetUser) {
    // Enumerate Processes across all sessions for JCPU/PCPU/WHAT
    PWTS_PROCESS_INFOW pProcInfo = nullptr;
    DWORD procCount = 0;
    std::unordered_map<DWORD, std::vector<ProcessInfo>> sessionProcesses;

    if (WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pProcInfo, &procCount)) {
        std::unique_ptr<WTS_PROCESS_INFOW, WtsDeleter> spProc(pProcInfo);
        for (DWORD i = 0; i < procCount; ++i) {
            DWORD sId = pProcInfo[i].SessionId;
            DWORD pId = pProcInfo[i].ProcessId;
            std::wstring pName = L"unknown";
            ULONGLONG totalCpu = 0;

            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pId);
            if (!hProc) hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pId);

            if (hProc) {
                WCHAR imagePath[MAX_PATH] = {};
                DWORD pathLen = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, imagePath, &pathLen)) {
                    std::wstring fullPath(imagePath, pathLen);
                    size_t slash = fullPath.find_last_of(L"/\\");
                    pName = (slash == std::wstring::npos) ? fullPath : fullPath.substr(slash + 1);
                }

                FILETIME ftCreate, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                    ULONGLONG kTime = ((ULONGLONG)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime;
                    ULONGLONG uTime = ((ULONGLONG)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime;
                    totalCpu = kTime + uTime;
                }
                CloseHandle(hProc);

                sessionProcesses[sId].push_back({ pId, pName, totalCpu });
            }
        }
    }

    std::vector<SessionData> userSessions;

    // Collect session details
    for (DWORD i = 0; i < sessionCount; ++i) {
        DWORD sId = pSessions[i].SessionId;

        LPWSTR pUserBuf = nullptr;
        DWORD bytesRet = 0;

        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sId, WTSUserName, &pUserBuf, &bytesRet)) {
            std::wstring user = pUserBuf ? pUserBuf : L"";
            WTSFreeMemory(pUserBuf);

            if (user.empty()) continue; // Skip unauthenticated/system sessions

            if (!targetUser.empty() && _wcsicmp(user.c_str(), targetUser.c_str()) != 0) {
                continue; // User filter
            }

            SessionData data = {};
            data.sessionId = sId;
            data.username = user;
            data.isConnected = (pSessions[i].State == WTSActive);

            // WinStation / TTY Name
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sId, WTSWinStationName, &pUserBuf, &bytesRet)) {
                data.tty = pUserBuf ? pUserBuf : L"console";
                WTSFreeMemory(pUserBuf);
            } else {
                data.tty = L"console";
            }

            // Client Address (FROM)
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sId, WTSClientAddress, &pUserBuf, &bytesRet)) {
                PWTS_CLIENT_ADDRESS pAddr = (PWTS_CLIENT_ADDRESS)pUserBuf;
                if (pAddr && pAddr->AddressFamily == AF_INET) {
                    char ipStr[64];
                    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d",
                        pAddr->Address[2], pAddr->Address[3], pAddr->Address[4], pAddr->Address[5]);
                    std::string s(ipStr);
                    data.fromHost = std::wstring(s.begin(), s.end());
                } else if (pAddr && pAddr->AddressFamily == AF_INET6) {
                    char ipStr[INET6_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET6, &pAddr->Address[2], ipStr, sizeof(ipStr));
                    std::string s(ipStr);
                    data.fromHost = std::wstring(s.begin(), s.end());
                } else {
                    data.fromHost = L"local";
                }
                WTSFreeMemory(pUserBuf);
            } else {
                data.fromHost = L"local";
            }

            // Session Timestamps (Logon, Idle)
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sId, WTSSessionInfo, &pUserBuf, &bytesRet)) {
                PWTSINFOW pInfo = (PWTSINFOW)pUserBuf;
                data.logonTime = pInfo->LogonTime.QuadPart;

                FILETIME ftCurrent;
                GetSystemTimeAsFileTime(&ftCurrent);
                ULONGLONG nowFt = ((ULONGLONG)ftCurrent.dwHighDateTime << 32) | ftCurrent.dwLowDateTime;
                ULONGLONG lastInFt = pInfo->LastInputTime.QuadPart;

                if (nowFt > lastInFt && lastInFt > 0) {
                    data.idleTimeMs = (nowFt - lastInFt) / 10000ULL;
                } else {
                    data.idleTimeMs = 0;
                }
                WTSFreeMemory(pUserBuf);
            }

            // Process CPU aggregation (JCPU, PCPU, WHAT)
            ULONGLONG totalJcpu = 0;
            ULONGLONG maxPcpu = 0;
            std::wstring topProc = L"cmd.exe";

            auto pIt = sessionProcesses.find(sId);
            if (pIt != sessionProcesses.end()) {
                for (const auto& proc : pIt->second) {
                    totalJcpu += proc.cpuTime;

                    // Exclude system background processes for 'WHAT'
                    std::wstring lowerName = proc.name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](wchar_t ch) {
                        return std::towlower(ch);
                    });

                    if (lowerName != L"explorer.exe" && lowerName != L"svchost.exe" && lowerName != L"csrss.exe") {
                        if (proc.cpuTime >= maxPcpu) {
                            maxPcpu = proc.cpuTime;
                            topProc = proc.name;
                        }
                    }
                }
            }

            data.jcpu100ns = totalJcpu;
            data.pcpu100ns = maxPcpu;
            data.whatProcess = topProc;

            userSessions.push_back(data);
        }
    }

    return userSessions;
}
