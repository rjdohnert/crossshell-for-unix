#include "wts_deleter.hpp"

void WtsDeleter::operator()(void* p) const {
        if (p) WTSFreeMemory(p);
    }
