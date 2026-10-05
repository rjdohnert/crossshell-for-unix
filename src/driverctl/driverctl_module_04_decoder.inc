// Decodes Windows CM_PROB_* codes into human-readable descriptions
class ProblemDecoder {
public:
    static std::string Decode(ULONG problemCode) {
        switch (problemCode) {
            case 0: return "Device is operating normally (No problem detected).";
            case CM_PROB_NOT_CONFIGURED: return "Code 1: Device is not configured correctly.";
            case CM_PROB_FAILED_START: return "Code 10: Device failed to start (Driver or Hardware malfunction).";
            case CM_PROB_OUT_OF_MEMORY: return "Code 3: Driver ran out of system memory.";
            case CM_PROB_NORMAL_CONFLICT: return "Code 12: Hardware resource conflict detected.";
            case CM_PROB_NEED_RESTART: return "Code 14: System restart required to complete driver initialization.";
            case CM_PROB_REINSTALL: return "Code 18: Drivers for this device must be reinstalled.";
            case CM_PROB_DISABLED: return "Code 22: Device is explicitly disabled by user/system.";
            case CM_PROB_FAILED_INSTALL: return "Code 28: No compatible driver installed for this hardware.";
            case CM_PROB_FAILED_ADD: return "Code 31: Device driver failed to load.";
            case CM_PROB_DISABLED_SERVICE: return "Code 32: The driver service is disabled in the registry.";
            case CM_PROB_DRIVER_FAILED_LOAD: return "Code 39: Driver binary corrupted or missing.";
            case CM_PROB_FAILED_POST_START: return "Code 43: Device reported a critical hardware/firmware failure.";
            default: return Fmt("Uncommon Hardware/Driver Error Code ({})", problemCode);
        }
    }
};
