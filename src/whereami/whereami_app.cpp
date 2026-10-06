#include "address_formatter.hpp"
#include "human_formatter.hpp"
#include "json_formatter.hpp"
#include "location_provider.hpp"
#include "location_report.hpp"
#include "network_geo_location_provider.hpp"
#include "output_formatter.hpp"
#include "raw_formatter.hpp"
#include "sexagesimal_formatter.hpp"
#include "system_context_collector.hpp"
#include "whereami_app.hpp"
#include "whereami_help.hpp"
#include "whereami_options.hpp"
#include "windows_sensor_location_provider.hpp"

int WhereAmIApp::parse_arguments(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                options.show_help = true;
                return 0;
            } else if (arg == "-V" || arg == "--version") {
                options.show_version = true;
                return 0;
            } else if (arg == "-j" || arg == "--json") {
                options.format = OutputFormat::Json;
            } else if (arg == "-s" || arg == "--sexagesimal") {
                options.format = OutputFormat::Sexagesimal;
            } else if (arg == "-a" || arg == "--accuracy") {
                options.show_accuracy = true;
            } else if (arg == "-A" || arg == "--altitude") {
                options.show_altitude = true;
            } else if (arg == "-d" || arg == "--address") {
                options.format = OutputFormat::Address;
                options.show_address = true;
            } else if (arg == "-n" || arg == "--network") {
                options.show_network = true;
            } else if (arg == "-v" || arg == "--verbose" || arg == "--debug") {
                options.verbose = true;
            } else if (arg == "-q" || arg == "--quiet") {
                options.quiet = true;
            } else if (arg == "-f" || arg.rfind("--format=", 0) == 0 || arg == "--format") {
                std::string fmt;
                if (arg.rfind("--format=", 0) == 0) {
                    fmt = arg.substr(9);
                } else if (i + 1 < argc) {
                    fmt = argv[++i];
                }
                if (fmt == "json") options.format = OutputFormat::Json;
                else if (fmt == "sexagesimal" || fmt == "dms") options.format = OutputFormat::Sexagesimal;
                else if (fmt == "human" || fmt == "verbose") options.format = OutputFormat::Human;
                else if (fmt == "address") options.format = OutputFormat::Address;
                else if (fmt == "raw") options.format = OutputFormat::Raw;
                else {
                    std::cerr << "whereami: unknown format '" << fmt << "'\n";
                    return 2;
                }
            } else if (arg == "-t" || arg.rfind("--timeout=", 0) == 0 || arg == "--timeout") {
                std::string t_str = (arg.rfind("--timeout=", 0) == 0) ? arg.substr(10) : argv[++i];
                try {
                    options.timeout_seconds = std::stoi(t_str);
                } catch (...) {
                    std::cerr << "whereami: invalid timeout value\n";
                    return 2;
                }
            } else if (arg.rfind("--from=", 0) == 0) {
                options.from_provider = arg.substr(7);
            } else if (arg.rfind("--basedir=", 0) == 0) {
                options.basedir = arg.substr(10);
            } else if (arg.rfind("--statedir=", 0) == 0) {
                options.statedir = arg.substr(11);
            } else {
                std::cerr << "whereami: unrecognized option '" << arg << "'\n";
                std::cerr << "usage: whereami [-aAdhjnqsvV] [-f format] [-t seconds] [--from provider]\n";
                return 2;
            }
        }
        return 0;
    }

int WhereAmIApp::execute() {
        if (options.show_help) {
            std::cout << BSD_MANUAL;
            return 0;
        }
        if (options.show_version) {
            std::cout << "whereami 2.0\n";
            return 0;
        }

        // Initialize Winsock for network querying
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        LocationReport report;
        std::vector<std::unique_ptr<ILocationProvider>> providers;

        if (options.from_provider == "sensor") {
            providers.push_back(std::make_unique<WindowsSensorLocationProvider>());
        } else if (options.from_provider == "network") {
            providers.push_back(std::make_unique<NetworkGeoLocationProvider>());
        } else {
            // Auto fallback chain: Sensor -> Network
            providers.push_back(std::make_unique<WindowsSensorLocationProvider>());
            providers.push_back(std::make_unique<NetworkGeoLocationProvider>());
        }

        bool acquired = false;
        for (const auto& provider : providers) {
            if (provider->query(report, options.timeout_seconds, options.verbose)) {
                acquired = true;
                break;
            }
        }

        if (!acquired) {
            if (!options.quiet) {
                std::cerr << "whereami: unable to ascertain current location";
                if (!report.error_message.empty()) {
                    std::cerr << " (" << report.error_message << ")";
                }
                std::cerr << "\n";
            }
            WSACleanup();
            return 1;
        }

        SystemContextCollector::populate(report);

        // Select formatter
        std::unique_ptr<IOutputFormatter> formatter;
        switch (options.format) {
            case OutputFormat::Json:
                formatter = std::make_unique<JsonFormatter>();
                break;
            case OutputFormat::Sexagesimal:
                formatter = std::make_unique<SexagesimalFormatter>();
                break;
            case OutputFormat::Human:
                formatter = std::make_unique<HumanFormatter>();
                break;
            case OutputFormat::Address:
                formatter = std::make_unique<AddressFormatter>();
                break;
            case OutputFormat::Raw:
            default:
                formatter = std::make_unique<RawFormatter>();
                break;
        }

        formatter->output(report, options);
        WSACleanup();
        return 0;
    }
