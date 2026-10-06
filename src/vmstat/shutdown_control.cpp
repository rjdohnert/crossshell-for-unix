#include "shutdown_control.hpp"

volatile bool g_running = true;
std::condition_variable g_shutdownCV;
std::mutex g_shutdownMutex;

BOOL WINAPI GlobalConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        g_running = false;
        g_shutdownCV.notify_all();
        return TRUE;
    }
    return FALSE;
}
