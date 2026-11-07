/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "driverctl_decoder.hpp"

#include <windows.h>
#include <cfgmgr32.h>
#include <string>

std::string DescribeProblemCode(unsigned long problemCode) {
    switch (problemCode) {
        case 0: return "Device is operating normally (no problem detected).";
        case CM_PROB_NOT_CONFIGURED: return "Device is not configured correctly.";
        case CM_PROB_FAILED_START: return "Device failed to start (driver or hardware malfunction).";
        case CM_PROB_OUT_OF_MEMORY: return "Driver ran out of system memory.";
        case CM_PROB_NORMAL_CONFLICT: return "Hardware resource conflict detected.";
        case CM_PROB_NEED_RESTART: return "System restart required to complete driver initialization.";
        case CM_PROB_REINSTALL: return "Drivers for this device must be reinstalled.";
        case CM_PROB_DISABLED: return "Device is explicitly disabled by user or system.";
        case CM_PROB_FAILED_INSTALL: return "No compatible driver is installed for this hardware.";
        case CM_PROB_FAILED_ADD: return "Device driver failed to load.";
        case CM_PROB_DISABLED_SERVICE: return "The driver service is disabled in the registry.";
        case CM_PROB_DRIVER_FAILED_LOAD: return "Driver binary is corrupted or missing.";
        case CM_PROB_FAILED_POST_START: return "Device reported a critical hardware or firmware failure.";
        default: return "Uncommon hardware/driver error code (" +
                      std::to_string(problemCode) + ").";
    }
}
