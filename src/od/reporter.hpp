#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "od.hpp"

class OdReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand);
};

#endif // REPORTER_HPP
