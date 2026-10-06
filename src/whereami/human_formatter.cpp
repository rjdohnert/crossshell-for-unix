#include "human_formatter.hpp"
#include "location_report.hpp"
#include "whereami_options.hpp"

void HumanFormatter::output(const LocationReport& report, const CliOptions& /*options*/)  {
        std::cout << "  Coordinates:  " << report.coords.to_raw()
                  << "  (" << report.coords.to_sexagesimal() << ")\n";

        if (report.coords.has_accuracy) {
            std::cout << "  Accuracy:     \xC2\xB1" << std::fixed << std::setprecision(0)
                      << report.coords.accuracy << " meters\n";
        }
        if (report.coords.has_altitude) {
            std::cout << "  Altitude:     " << std::fixed << std::setprecision(1)
                      << report.coords.altitude << " meters\n";
        }
        if (!report.city.empty()) {
            std::cout << "  Address:      " << report.city << ", "
                      << report.region_name << " " << report.postal_code << ", "
                      << report.country << " (" << report.country_code << ")\n";
        }
        if (!report.timezone.empty()) {
            std::cout << "  Timezone:     " << report.timezone << "\n";
        }
        if (!report.public_ip.empty() || !report.local_ip.empty()) {
            std::cout << "  Network:      Local: " << (report.local_ip.empty() ? "N/A" : report.local_ip)
                      << " | External: " << (report.public_ip.empty() ? "N/A" : report.public_ip)
                      << " (" << report.isp << ")\n";
        }
        if (!report.hostname.empty()) {
            std::cout << "  Hostname:     " << report.hostname << "\n";
        }
        std::cout << "  Provider:     " << report.provider_name << "\n";
    }
