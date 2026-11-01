#ifndef COMM_ARG_PARSER_H
#define COMM_ARG_PARSER_H

#include "comm.h"
#include "options.h"

namespace CommUtil {

// Command Line Argument Parser
class ArgParser {
public:
    static bool parse(int argc, char* argv[], Options& opts);
};

} // namespace CommUtil

#endif // COMM_ARG_PARSER_H
