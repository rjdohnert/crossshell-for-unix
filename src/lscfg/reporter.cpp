#include "reporter.hpp"
#include <iostream>
#include <iomanip>

static std::string QuoteCsv(const std::string& value) {
    std::string out = "\"";
    for (char c : value) out += c == '"' ? "\"\"" : std::string(1, c);
    return out + '"';
}

static std::string EscapeJson(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

void LscfgReporter::Report(const std::vector<HardwareDevice>& devices, const LscfgOptions& opts) {
    if (opts.format == OutputFormat::Csv) {
        std::cout << "Resource,Location,Description,Class\n";
        for (const auto& dev : devices) {
            std::cout << QuoteCsv(dev.resourceName) << ','
                      << QuoteCsv(dev.location) << ','
                      << QuoteCsv(dev.description) << ','
                      << QuoteCsv(dev.deviceClass) << '\n';
        }
        return;
    }

    if (opts.format == OutputFormat::Json) {
        std::cout << "[\n";
        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& dev = devices[i];
            std::cout << "  {\"resource\":\"" << EscapeJson(dev.resourceName)
                      << "\",\"location\":\"" << EscapeJson(dev.location)
                      << "\",\"description\":\"" << EscapeJson(dev.description)
                      << "\",\"class\":\"" << EscapeJson(dev.deviceClass) << "\"}"
                      << (i + 1 == devices.size() ? "\n" : ",\n");
        }
        std::cout << "]\n";
        return;
    }

    // Print Header
    std::cout << "\nINSTALLED RESOURCE LIST\n\n";
    std::cout << "The following resources are installed on the system.\n";
    std::cout << "+/- = Added/Removed from last database update.\n\n";
    std::cout << "  RESOURCE        LOCATION                  DESCRIPTION\n";
    std::cout << "  -----------------------------------------------------------------------------\n";

    if (devices.empty()) {
        std::cout << "  No matching hardware resources found for filter: '" << opts.filter << "'\n\n";
        return;
    }

    // Output Device Table
    for (const auto& dev : devices) {
        std::cout << "* " << std::left << std::setw(14) << dev.resourceName
                  << " " << std::setw(25) << dev.location
                  << " " << dev.description << "\n";

        // Print Vital Product Data (VPD) if Verbose (-v) is set and summaryOnly is false
        if (opts.verbose && !opts.summaryOnly) {
            for (const auto& vpd : dev.vpdAttributes) {
                std::string keyLabel = vpd.key;
                int dots = 30 - static_cast<int>(keyLabel.length());
                if (dots < 2) dots = 2;
                std::string dotPadding(dots, '.');

                std::cout << "        " << keyLabel << dotPadding << " " << vpd.value << "\n";
            }
            std::cout << "\n";
        }
    }
    std::cout << "\n";
}
