#ifndef COMM_APP_H
#define COMM_APP_H

#include "comm.h"

namespace CommUtil {

// Main Application Coordinator
class CommApp {
public:
    static int run(int argc, char* argv[]);
};

} // namespace CommUtil

#endif // COMM_APP_H
