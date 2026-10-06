#pragma once

#include "traceroute_options.hpp"
#include "traceroute.hpp"

class TracerouteEngine {
public:
    explicit TracerouteEngine(const TracerouteOptions& opts);

    bool Execute();

private:
    const TracerouteOptions& m_opts;
};
