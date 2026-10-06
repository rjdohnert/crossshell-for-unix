#pragma once

#include "vmstat.hpp"

struct PdhRateSample {
    bool anyValid = false;
    bool fltValid = false;
    bool pagesInValid = false;
    bool pagesOutValid = false;
    bool interruptsValid = false;
    bool sysCallsValid = false;
    bool ctxSwitchValid = false;
    double fltPerSec = 0.0;
    double pagesInPerSec = 0.0;
    double pagesOutPerSec = 0.0;
    double interruptsPerSec = 0.0;
    double sysCallsPerSec = 0.0;
    double ctxSwitchPerSec = 0.0;
};
