#pragma once

#include "coordinates.hpp"
#include "whereami.hpp"

struct LocationReport {
    Coordinates coords;
    std::string country;
    std::string country_code;
    std::string region;
    std::string region_name;
    std::string city;
    std::string postal_code;
    std::string timezone;
    std::string isp;
    std::string public_ip;
    std::string local_ip;
    std::string hostname;
    std::string provider_name;
    bool success = false;
    std::string error_message;
};
