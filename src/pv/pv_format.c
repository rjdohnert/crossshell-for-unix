#include "pv_internal.h"

uint64_t parse_size_suffix(const char *str) {
    char *endptr;
    double val = strtod(str, &endptr);
    if (endptr == str) return 0;
    while (*endptr == ' ') endptr++;
    switch (*endptr) {
        case 'k': case 'K': val *= 1024ULL; break;
        case 'm': case 'M': val *= 1024ULL * 1024ULL; break;
        case 'g': case 'G': val *= 1024ULL * 1024ULL * 1024ULL; break;
        case 't': case 'T': val *= 1024ULL * 1024ULL * 1024ULL * 1024ULL; break;
        case 'p': case 'P': val *= 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL; break;
    }
    return (uint64_t)val;
}

void format_units(char *buf, size_t sz, uint64_t count, bool bits, bool lines) {
    if (lines) {
        snprintf(buf, sz, "%llu lines", (unsigned long long)count);
        return;
    }
    double val = (double)count;
    const char *unit = bits ? "b" : "B";
    const char *prefixes[] = {"", "Ki", "Mi", "Gi", "Ti", "Pi"};
    int idx = 0;
    if (bits) val *= 8.0;

    while (val >= 1024.0 && idx < 5) {
        val /= 1024.0;
        idx++;
    }

    if (idx == 0)
        snprintf(buf, sz, "%4.0f %s%s", val, prefixes[idx], unit);
    else if (val < 10.0)
        snprintf(buf, sz, "%4.2f %s%s", val, prefixes[idx], unit);
    else if (val < 100.0)
        snprintf(buf, sz, "%4.1f %s%s", val, prefixes[idx], unit);
    else
        snprintf(buf, sz, "%4.0f %s%s", val, prefixes[idx], unit);
}

void format_time(char *buf, size_t sz, double sec) {
    if (sec < 0 || sec > 8640000.0) {
        snprintf(buf, sz, "--:--:--");
        return;
    }
    int s = (int)sec;
    int h = s / 3600;
    int m = (s % 3600) / 60;
    s = s % 60;
    snprintf(buf, sz, "%02d:%02d:%02d", h, m, s);
}
