#include "location_report.hpp"
#include "sexagesimal_formatter.hpp"
#include "whereami_options.hpp"

void SexagesimalFormatter::output(const LocationReport& report, const CliOptions& options)  {
        std::cout << report.coords.to_sexagesimal();
        if (options.show_altitude && report.coords.has_altitude) {
            std::cout << " (Alt: " << std::fixed << std::setprecision(1) << report.coords.altitude << "m)";
        }
        if (options.show_accuracy && report.coords.has_accuracy) {
            std::cout << " [Acc: \xC2\xB1" << std::fixed << std::setprecision(0) << report.coords.accuracy << "m]";
        }
        std::cout << "\n";
    }
