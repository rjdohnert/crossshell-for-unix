#include "service_events.hpp"
#include "supervisor.hpp"

void WriteServiceEvent(const std::string& message, WORD type ) {
    if (!g_IsServiceMode) return;

    HANDLE hEventLog = RegisterEventSourceA(NULL, "Supervisord");
    if (!hEventLog) return;

    LPCSTR strings[1] = { message.c_str() };
    ReportEventA(hEventLog, type, 0, 0x1000, NULL, 1, 0, strings, NULL);
    DeregisterEventSource(hEventLog);
}
