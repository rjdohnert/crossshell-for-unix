#pragma once

#include "location_provider.hpp"
#include "location_report.hpp"
#include "whereami.hpp"

class NetworkGeoLocationProvider : public ILocationProvider {
public:
    std::string get_name() const override;

    bool query(LocationReport& out_report, int timeout_seconds, bool verbose) override;
};
