#pragma once

#include "location_report.hpp"
#include "whereami.hpp"

class SystemContextCollector {
public:
    static void populate(LocationReport& report);
};
