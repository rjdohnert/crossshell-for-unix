#include "json_formatter.hpp"
#include "location_report.hpp"
#include "whereami_options.hpp"

void JsonFormatter::output(const LocationReport& report, const CliOptions& options)  {
        std::cout << "{\n";
        std::cout << "  \"latitude\": " << std::fixed << std::setprecision(6) << report.coords.latitude << ",\n";
        std::cout << "  \"longitude\": " << std::fixed << std::setprecision(6) << report.coords.longitude;

        if (report.coords.has_altitude || options.show_altitude) {
            std::cout << ",\n  \"altitude\": " << std::fixed << std::setprecision(1) << report.coords.altitude;
        }
        if (report.coords.has_accuracy || options.show_accuracy) {
            std::cout << ",\n  \"accuracy\": " << std::fixed << std::setprecision(1) << report.coords.accuracy;
        }

        if (options.show_address || !report.city.empty()) {
            std::cout << ",\n  \"city\": \"" << report.city << "\"";
            std::cout << ",\n  \"region\": \"" << report.region_name << "\"";
            std::cout << ",\n  \"country\": \"" << report.country << "\"";
            std::cout << ",\n  \"postal_code\": \"" << report.postal_code << "\"";
        }

        if (options.show_network || !report.public_ip.empty()) {
            std::cout << ",\n  \"public_ip\": \"" << report.public_ip << "\"";
            std::cout << ",\n  \"local_ip\": \"" << report.local_ip << "\"";
            std::cout << ",\n  \"isp\": \"" << report.isp << "\"";
            std::cout << ",\n  \"hostname\": \"" << report.hostname << "\"";
        }

        std::cout << ",\n  \"provider\": \"" << report.provider_name << "\"\n";
        std::cout << "}\n";
    }
