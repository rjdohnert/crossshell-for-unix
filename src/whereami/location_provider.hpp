#pragma once

#include "location_report.hpp"
#include "whereami.hpp"

class ILocationProvider {
public:
    virtual ~ILocationProvider() = default;
    virtual std::string get_name() const = 0;
    virtual bool query(LocationReport& out_report, int timeout_seconds, bool verbose) = 0;
};
