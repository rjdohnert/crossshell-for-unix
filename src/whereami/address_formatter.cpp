#include "address_formatter.hpp"
#include "location_report.hpp"
#include "whereami_options.hpp"

void AddressFormatter::output(const LocationReport& report, const CliOptions& /*options*/)  {
        if (!report.city.empty()) {
            std::cout << report.city << ", " << report.region_name;
            if (!report.postal_code.empty()) std::cout << " " << report.postal_code;
            std::cout << ", " << report.country << "\n";
        } else {
            std::cout << report.coords.to_raw() << "\n";
        }
    }
