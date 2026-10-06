#pragma once

#include "w.hpp"

struct WtsDeleter {
    void operator()(void* p) const;
};
