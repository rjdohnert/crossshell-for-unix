#include "session_broadcaster.hpp"
#include "system_info_provider.hpp"

bool SessionBroadcaster::broadcastWts(HANDLE hServer, const std::string& title, const std::string& message, bool isGui) {
        PWTS_SESSION_INFOW pSessionInfo = NULL;
        DWORD count = 0;

        if (!WTSEnumerateSessionsW(hServer, 0, 1, &pSessionInfo, &count)) {
            return false;
        }

        std::wstring wTitle = SystemInfoProvider::utf8ToWide(title);
        std::wstring wMsg = SystemInfoProvider::utf8ToWide(message);
        DWORD style = isGui ? (MB_OK | MB_ICONEXCLAMATION) : (MB_OK | MB_ICONASTERISK);
        DWORD response = 0;

        for (DWORD i = 0; i < count; ++i) {
            if (pSessionInfo[i].State == WTSActive || pSessionInfo[i].State == WTSConnected) {
                WTSSendMessageW(
                    hServer,
                    pSessionInfo[i].SessionId,
                    wTitle.data(),
                    static_cast<DWORD>(wTitle.size() * sizeof(wchar_t)),
                    wMsg.data(),
                    static_cast<DWORD>(wMsg.size() * sizeof(wchar_t)),
                    style,
                    0,
                    &response,
                    FALSE
                );
            }
        }

        WTSFreeMemory(pSessionInfo);
        return true;
    }
