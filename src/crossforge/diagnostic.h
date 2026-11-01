#ifndef CROSSFORGE_DIAGNOSTIC_H
#define CROSSFORGE_DIAGNOSTIC_H

#include "crossforge.h"

struct Diagnostic {
    std::string FilePath;
    int Line = 0;
    std::string Message;
};

#endif // CROSSFORGE_DIAGNOSTIC_H
