#ifndef COMM_HELP_SYSTEM_H
#define COMM_HELP_SYSTEM_H

#include "comm.h"

namespace CommUtil {

// Help & Information Subsystem
class HelpSystem {
public:
    static void print_version();
    static void print_help();
};

} // namespace CommUtil

#endif // COMM_HELP_SYSTEM_H
