#include "pv_internal.h"

int64_t get_qpc_frequency(void) {
    static int64_t freq = 0;
    if (freq == 0) {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        freq = li.QuadPart;
    }
    return freq;
}

double get_time_sec(void) {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (double)li.QuadPart / (double)get_qpc_frequency();
}
