#pragma once

#include "location_report.hpp"
#include "output_formatter.hpp"
#include "whereami_options.hpp"
#include "whereami.hpp"

class RawFormatter : public IOutputFormatter {
public:
    void output(const LocationReport& report, const CliOptions& options) override;
};
