#ifndef MTR_REPORTER_HPP
#define MTR_REPORTER_HPP

#include "mtr.hpp"
#include "options.hpp"

class MtrDisplay {
public:
    static void render(const MtrOptions& options,
                       const std::string& targetIp,
                       const std::vector<HopRecord>& hops,
                       size_t cycleCount,
                       int currentHop,
                       bool paused);
};

#endif // MTR_REPORTER_HPP
