#include "process_tracker.hpp"
#include "symbol_resolver.hpp"

ProcessTracker::~ProcessTracker() {
    CleanupAll();
}

void ProcessTracker::CleanupAll() {
    for (auto& entry : processHandles) {
        if (entry.second) {
            SymbolResolver::Cleanup(entry.second);
            CloseHandle(entry.second);
        }
    }
    processHandles.clear();

    for (auto& entry : threadHandles) {
        if (entry.second) {
            CloseHandle(entry.second);
        }
    }
    threadHandles.clear();
}
