#ifndef SHA256_REPORTER_HPP
#define SHA256_REPORTER_HPP

#include "sha256sum.hpp"

class Sha256Reporter {
public:
    static int dispatch(const std::vector<Sha256Result>& results, int format, const std::string& pipeCommand);
};

#endif // SHA256_REPORTER_HPP
