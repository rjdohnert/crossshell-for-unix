/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "driverctl_app.hpp"

#include "driverctl_formatters.hpp"
#include "driverctl_logger.hpp"
#include "driverctl_models.hpp"
#include "driverctl_scanner.hpp"
#include "driverctl_utilities.hpp"

#include <windows.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void ShowHelp() {
    std::cout << R"(driverctl(1)               CrossShell for UNIX Reference Manual               driverctl(1)

    NAME
        driverctl - inspect and diagnose Windows PnP device drivers

    SYNOPSIS
        driverctl [COMMAND] [OPTIONS]

    DESCRIPTION
        Interfaces with the Windows PnP Configuration Manager to inspect device
        drivers and diagnose hardware malfunctions, start errors, and disabled
        devices. When stdin is piped, driverctl reads a category filter from it.

    COMMANDS
        --list
            Enumerate devices, categories, and drivers.
        --test
            Run live hardware and driver malfunction diagnostics.

    OPTIONS
        --category NAME
            Filter devices by setup class/category (e.g., Display, Net, USB).
        --errors-only
            With --test, suppress healthy devices.
        --table
            Emit results as an aligned table on stdout.
        --json
            Emit results as a JSON array on stdout.
        --csv
            Emit results as CSV on stdout.
        -h, --help
            Display this reference manual and exit.

    EXAMPLES
        driverctl --list
            Enumerate all installed devices and drivers.

        driverctl --test --category Display
            Inspect display/graphics drivers and test for malfunctions.

        driverctl --test --errors-only
            Run diagnostics and only display non-healthy devices.

        driverctl --list --json
            Export device inventory formatted as JSON.

    CrossShell for UNIX                                                    driverctl(1)
)";
}

void RenderTable(const std::vector<DeviceRecord>& records, bool runTest) {
    auto join = [](const std::vector<std::string>& values) {
        return values.empty() ? std::string("-") : JoinList(values);
    };
    std::cout << std::left
              << std::setw(32) << "Device Name" << "  "
              << std::setw(18) << "Category" << "  "
              << std::setw(28) << "Driver Type" << "  "
              << std::setw(20) << "Service" << "  "
              << std::setw(20) << "Upper Filters" << "  "
              << std::setw(20) << "Lower Filters";
    if (runTest) std::cout << "  " << std::setw(12) << "Health";
    std::cout << '\n';

    for (const auto& record : records) {
        std::cout << std::left
                  << std::setw(32) << record.deviceName.substr(0, 31) << "  "
                  << std::setw(18) << record.category.substr(0, 17) << "  "
                  << std::setw(28) << record.driverType.substr(0, 27) << "  "
                  << std::setw(20) << (record.serviceName.empty()
                      ? "-" : ToUtf8(record.serviceName)).substr(0, 19) << "  "
                  << std::setw(20) << join(record.upperFilters).substr(0, 19) << "  "
                  << std::setw(20) << join(record.lowerFilters).substr(0, 19);
        if (runTest) {
            std::cout << "  " << std::setw(12) << HealthToString(record.health);
        }
        std::cout << '\n';
    }
}

void RenderJson(const std::vector<DeviceRecord>& records, bool runTest) {
    std::cout << "[\n";
    for (size_t index = 0; index < records.size(); ++index) {
        const auto& record = records[index];
        auto jsonList = [](const std::vector<std::string>& values) {
            std::string output = "[";
            for (size_t valueIndex = 0; valueIndex < values.size(); ++valueIndex) {
                if (valueIndex > 0) output += ", ";
                output += "\"" + EscapeJSON(values[valueIndex]) + "\"";
            }
            output += "]";
            return output;
        };

        std::cout << "  {\n"
                  << "    \"deviceName\": \"" << EscapeJSON(record.deviceName) << "\",\n"
                  << "    \"category\": \"" << EscapeJSON(record.category) << "\",\n"
                  << "    \"driverType\": \"" << EscapeJSON(record.driverType) << "\",\n"
                  << "    \"driverProvider\": \"" << EscapeJSON(record.driverProvider) << "\",\n"
                  << "    \"driverVersion\": \"" << EscapeJSON(record.driverVersion) << "\",\n"
                  << "    \"serviceName\": \"" << EscapeJSON(ToUtf8(record.serviceName)) << "\",\n"
                  << "    \"upperFilters\": " << jsonList(record.upperFilters) << ",\n"
                  << "    \"lowerFilters\": " << jsonList(record.lowerFilters);
        if (runTest) {
            std::cout << ",\n"
                      << "    \"health\": \"" << HealthToString(record.health) << "\",\n"
                      << "    \"problemCode\": " << record.problemCode << ",\n"
                      << "    \"diagnosticDetails\": \""
                      << EscapeJSON(record.diagnosticDetails) << "\"\n";
        } else {
            std::cout << '\n';
        }
        std::cout << "  }" << (index + 1 < records.size() ? "," : "") << '\n';
    }
    std::cout << "]\n";
}

void RenderCsv(const std::vector<DeviceRecord>& records, bool runTest) {
    std::cout << "DeviceName,Category,DriverType,DriverProvider,DriverVersion,"
                 "ServiceName,UpperFilters,LowerFilters";
    if (runTest) std::cout << ",Health,ProblemCode,DiagnosticDetails";
    std::cout << '\n';

    auto joinSemi = [](const std::vector<std::string>& values) {
        std::string output;
        for (size_t index = 0; index < values.size(); ++index) {
            if (index > 0) output += ';';
            output += values[index];
        }
        return output;
    };
    for (const auto& record : records) {
        std::cout << EscapeCSV(record.deviceName) << ','
                  << EscapeCSV(record.category) << ','
                  << EscapeCSV(record.driverType) << ','
                  << EscapeCSV(record.driverProvider) << ','
                  << EscapeCSV(record.driverVersion) << ','
                  << EscapeCSV(ToUtf8(record.serviceName)) << ','
                  << EscapeCSV(joinSemi(record.upperFilters)) << ','
                  << EscapeCSV(joinSemi(record.lowerFilters));
        if (runTest) {
            std::cout << ',' << EscapeCSV(HealthToString(record.health))
                      << ',' << record.problemCode
                      << ',' << EscapeCSV(record.diagnosticDetails);
        }
        std::cout << '\n';
    }
}

bool IsInputPiped() {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == nullptr || input == INVALID_HANDLE_VALUE) return false;
    DWORD type = GetFileType(input);
    return type != FILE_TYPE_CHAR && type != FILE_TYPE_UNKNOWN;
}

std::string ReadPipedFirstFilter() {
    std::string token;
    if (std::cin >> token) return token;
    return {};
}

bool HasUnhealthyStatus(const DeviceRecord& record) {
    return record.health != HealthStatus::Healthy;
}

} // namespace

int DriverCtlApp::Run(int argc, char* argv[]) {
    bool runTest = false;
    bool listOnly = false;
    bool errorsOnly = false;
    OutputFormat format = OutputFormat::Report;
    std::string categoryFilter;

    for (int index = 1; index < argc; ++index) {
        if (!argv[index]) {
            std::cerr << "[driverctl] Error: received an invalid argument.\n";
            return 2;
        }
        std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            ShowHelp();
            return 0;
        }
        if (argument == "--test") {
            runTest = true;
        } else if (argument == "--list") {
            listOnly = true;
        } else if (argument == "--errors-only") {
            errorsOnly = true;
        } else if (argument == "--table") {
            format = OutputFormat::Table;
        } else if (argument == "--json") {
            format = OutputFormat::Json;
        } else if (argument == "--csv") {
            format = OutputFormat::Csv;
        } else if (argument == "--category") {
            if (index + 1 >= argc || !argv[index + 1] ||
                argv[index + 1][0] == '\0' || argv[index + 1][0] == '-') {
                std::cerr << "[driverctl] Error: --category requires a non-empty name.\n";
                return 2;
            }
            categoryFilter = argv[++index];
        } else {
            std::cerr << "[driverctl] Error: unknown option or argument: "
                      << argument << '\n';
            return 2;
        }
    }

    if (errorsOnly && !runTest) {
        std::cerr << "[driverctl] Error: --errors-only requires --test.\n";
        return 2;
    }
    if (!runTest && !listOnly) {
        ShowHelp();
        return 0;
    }

    if (categoryFilter.empty() && IsInputPiped()) {
        categoryFilter = ReadPipedFirstFilter();
    }

    DeviceScanner scanner;
    ScanResult scan = scanner.Scan(runTest, categoryFilter);
    if (!scan) {
        std::cerr << "[driverctl] Error: device scan failed: " << scan.error << '\n';
        return 1;
    }
    const size_t scannedDeviceCount = scan.devices.size();
    size_t healthyCount = 0;
    size_t warningCount = 0;
    size_t errorCount = 0;
    size_t disabledCount = 0;
    if (runTest) {
        for (const auto& record : scan.devices) {
            switch (record.health) {
                case HealthStatus::Healthy: ++healthyCount; break;
                case HealthStatus::Warning: ++warningCount; break;
                case HealthStatus::Malfunction: ++errorCount; break;
                case HealthStatus::Disabled: ++disabledCount; break;
            }
        }
    }
    if (errorsOnly) {
        scan.devices.erase(std::remove_if(scan.devices.begin(), scan.devices.end(),
            [](const DeviceRecord& record) { return !HasUnhealthyStatus(record); }),
            scan.devices.end());
    }

    if (format == OutputFormat::Table) {
        RenderTable(scan.devices, runTest);
        return std::cout ? 0 : 1;
    }
    if (format == OutputFormat::Json) {
        RenderJson(scan.devices, runTest);
        return std::cout ? 0 : 1;
    }
    if (format == OutputFormat::Csv) {
        RenderCsv(scan.devices, runTest);
        return std::cout ? 0 : 1;
    }

    ReportLogger logger;
    logger.WriteLine("");
    logger.WriteLine("                      Driverctl Device Diagnostic Report                      ");
    logger.WriteLine("");
    ElevationStatus elevation = GetElevationStatus();
    if (!elevation.querySucceeded) {
        logger.WriteLine(Fmt("WARNING: Could not determine elevation status: {}",
                             FormatWin32Error(elevation.errorCode)));
    } else if (!elevation.elevated) {
        logger.WriteLine("NOTE: Not running as administrator - some device status and problem");
        logger.WriteLine("      codes may be incomplete. Re-run elevated for full diagnostics.");
        logger.WriteLine("");
    }
    std::string logPath = logger.GetPath().empty()
        ? std::string("unavailable")
        : ToUtf8(logger.GetPath().wstring());
    logger.WriteLine(Fmt("Log Path: {}", logPath));
    if (!categoryFilter.empty()) {
        logger.WriteLine(Fmt("Filter: Category matching '{}'", categoryFilter));
    }
    logger.WriteLine("\n");

    for (const auto& record : scan.devices) {
        if (errorsOnly && record.health == HealthStatus::Healthy) continue;

        logger.WriteLine(Fmt("Device Name     : {}", record.deviceName));
        logger.WriteLine(Fmt("Category        : {}", record.category));
        logger.WriteLine(Fmt("Driver Type     : {}", record.driverType));
        logger.WriteLine(Fmt("Driver Provider : {}",
            record.driverProvider.empty() ? "N/A" : record.driverProvider));
        logger.WriteLine(Fmt("Driver Version  : {}",
            record.driverVersion.empty() ? "N/A" : record.driverVersion));
        logger.WriteLine(Fmt("Service Name    : {}",
            record.serviceName.empty() ? "None (Direct/Bus)" : ToUtf8(record.serviceName)));
        if (!record.upperFilters.empty()) {
            logger.WriteLine(Fmt("Upper Filters   : {}", JoinList(record.upperFilters)));
        }
        if (!record.lowerFilters.empty()) {
            logger.WriteLine(Fmt("Lower Filters   : {}", JoinList(record.lowerFilters)));
        }
        if (runTest) {
            std::string statusTag;
            switch (record.health) {
                case HealthStatus::Healthy: statusTag = "[PASS] HEALTHY"; break;
                case HealthStatus::Warning: statusTag = "[WARN] WARNING"; break;
                case HealthStatus::Malfunction: statusTag = "[FAIL] MALFUNCTION DETECTED"; break;
                case HealthStatus::Disabled: statusTag = "[INFO] DISABLED"; break;
            }
            logger.WriteLine(Fmt("Test Verdict    : {}", statusTag));
            logger.WriteLine(Fmt("Diagnostic Note : {}", record.diagnosticDetails));
        }
        logger.WriteLine("-------------------------------------------------------------------------------");
    }

    logger.WriteLine("\n");
    logger.WriteLine("                               Diagnostic Summary                            ");
    logger.WriteLine("");
    logger.WriteLine(Fmt("Total Devices Scanned : {}", scannedDeviceCount));
    if (runTest) {
        logger.WriteLine(Fmt("Operational / Healthy : {}", healthyCount));
        logger.WriteLine(Fmt("Warnings / Deferred   : {}", warningCount));
        logger.WriteLine(Fmt("Malfunctions / Errors : {}", errorCount));
        logger.WriteLine(Fmt("Disabled Devices      : {}", disabledCount));
    } else {
        logger.WriteLine("Diagnostic Health     : Not tested (use --test)");
    }
    logger.WriteLine("\n");
    bool logComplete = logger.Finish();
    if (logComplete) {
        std::cout << Fmt("Complete report saved to: {}", logPath) << '\n';
    } else {
        std::cout << "Diagnostic log is incomplete or unavailable; "
                     "the report was printed to stdout.\n";
    }

    return std::cout && logComplete ? 0 : 1;
}
