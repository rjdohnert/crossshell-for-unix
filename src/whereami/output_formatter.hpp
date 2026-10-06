#pragma once

#include "location_report.hpp"
#include "whereami_options.hpp"
#include "whereami.hpp"

class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual void output(const LocationReport& report, const CliOptions& options) = 0;
};
