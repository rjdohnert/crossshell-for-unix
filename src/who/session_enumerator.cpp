#include "scoped_wts_memory.hpp"
#include "session_enumerator.hpp"
#include "user_session.hpp"

std::wstring SessionEnumerator::FormatFileTime(const FILETIME& ft) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

        wchar_t buf[64];
        swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d",
                  stLocal.wYear, stLocal.wMonth, stLocal.wDay,
                  stLocal.wHour, stLocal.wMinute);
        return buf;
    }

std::wstring SessionEnumerator::GetBootTime() {
        ULONGLONG uptimeMS = GetTickCount64();
        FILETIME ftNow;
        GetSystemTimeAsFileTime(&ftNow);

        ULARGE_INTEGER uli;
        uli.LowPart = ftNow.dwLowDateTime;
        uli.HighPart = ftNow.dwHighDateTime;
        uli.QuadPart -= (uptimeMS * 10000ULL);

        FILETIME ftBoot;
        ftBoot.dwLowDateTime = uli.LowPart;
        ftBoot.dwHighDateTime = uli.HighPart;

        return FormatFileTime(ftBoot);
    }

std::wstring SessionEnumerator::GetCurrentUserName() {
        wchar_t buf[256] = { 0 };
        DWORD size = 256;
        if (GetUserNameW(buf, &size)) {
            return buf;
        }
        return L"";
    }

std::vector<UserSession> SessionEnumerator::GetLoggedOnUsers() {
        std::vector<UserSession> sessions;
        PWTS_SESSION_INFOW pSessionInfo = nullptr;
        DWORD count = 0;

        if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
            return sessions;
        }
        ScopedWtsMemory<WTS_SESSION_INFOW> sessionListHolder(pSessionInfo);

        for (DWORD i = 0; i < count; ++i) {
            PWSTR pUserName = nullptr;
            PWSTR pDomain = nullptr;
            PWSTR pWinStation = nullptr;
            PWSTR pClientName = nullptr;
            DWORD bytesReturned = 0;

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSUserName, &pUserName, &bytesReturned);
            ScopedWtsMemory<wchar_t> userHolder(pUserName);

            if (!pUserName || wcslen(pUserName) == 0) {
                continue;
            }

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSDomainName, &pDomain, &bytesReturned);
            ScopedWtsMemory<wchar_t> domainHolder(pDomain);

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSWinStationName, &pWinStation, &bytesReturned);
            ScopedWtsMemory<wchar_t> stationHolder(pWinStation);

            WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSClientName, &pClientName, &bytesReturned);
            ScopedWtsMemory<wchar_t> clientHolder(pClientName);

            PWTSINFOW pInfo = nullptr;
            std::wstring timeStr = L"-";
            if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionListHolder.Get()[i].SessionId, WTSSessionInfo, reinterpret_cast<PWSTR*>(&pInfo), &bytesReturned)) {
                ScopedWtsMemory<WTSINFOW> infoHolder(pInfo);
                if (pInfo && pInfo->LogonTime.QuadPart > 0) {
                    FILETIME ft;
                    ft.dwLowDateTime = pInfo->LogonTime.LowPart;
                    ft.dwHighDateTime = pInfo->LogonTime.HighPart;
                    timeStr = FormatFileTime(ft);
                }
            }

            UserSession session;
            session.username = pUserName ? pUserName : L"";
            session.domain = pDomain ? pDomain : L"";
            session.line = (pWinStation && wcslen(pWinStation) > 0) ? pWinStation : L"console";
            session.clientName = (pClientName && wcslen(pClientName) > 0) ? pClientName : L"local";
            session.logonTime = timeStr;
            session.isActive = (sessionListHolder.Get()[i].State == WTSActive);

            sessions.push_back(session);
        }

        return sessions;
    }
