#ifndef COMM_EXIT_CODE_H
#define COMM_EXIT_CODE_H

#include "comm.h"

namespace CommUtil {

// Program Exit Codes
enum class ExitCode : int {
    Success = 0,
    Failure = 1
};

} // namespace CommUtil

#endif // COMM_EXIT_CODE_H
