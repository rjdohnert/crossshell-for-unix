#include "process_state.hpp"

std::string StateToString(ProcessState state) {
    switch (state) {
        case ProcessState::STOPPED:  return "STOPPED";
        case ProcessState::STARTING: return "STARTING";
        case ProcessState::RUNNING:  return "RUNNING";
        case ProcessState::BACKOFF:  return "BACKOFF";
        case ProcessState::STOPPING: return "STOPPING";
        case ProcessState::FATAL:    return "FATAL";
        default:                     return "UNKNOWN";
    }
}
