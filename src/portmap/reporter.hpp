#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "portmap.hpp"
#include "options.hpp"

class PortmapReporter {
public:
    static void Emit(const PortmapOptions& cfg, const std::vector<ConnectionEntry>& entries);
};

#endif // REPORTER_HPP
