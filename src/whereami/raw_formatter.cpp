#include "location_report.hpp"
#include "raw_formatter.hpp"
#include "whereami_options.hpp"

void RawFormatter::output(const LocationReport& report, const CliOptions& options)  {
        std::cout << report.coords.to_raw();
        if (options.show_altitude && report.coords.has_altitude) {
            std::cout << "," << std::fixed << std::setprecision(1) << report.coords.altitude;
        }
        if (options.show_accuracy && report.coords.has_accuracy) {
            std::cout << " (+/-" << std::fixed << std::setprecision(0) << report.coords.accuracy << "m)";
        }
        std::cout << "\n";
    }
