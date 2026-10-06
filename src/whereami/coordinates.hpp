#pragma once

#include "whereami.hpp"

struct Coordinates {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double accuracy = 0.0;
    bool has_altitude = false;
    bool has_accuracy = false;

    std::string to_raw() const;

    std::string to_sexagesimal() const;
};
