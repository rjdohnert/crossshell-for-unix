// Application Controller & CLI Interface
class DriverCtlApp {
private:
    DeviceScanner scanner;
    // Lazily constructed so structured output paths (--json/--csv/--table) never
    // touch the disk. Only created when the human-readable Report path runs.
    std::unique_ptr<ReportLogger> logger;

    // Renders records as an aligned table on stdout (pipeline/parse friendly).
    static void RenderTable(const std::vector<DeviceRecord>& records, bool runTest) {
        auto join = [](const std::vector<std::string>& v) { return v.empty() ? std::string("-") : JoinList(v); };
        std::cout << std::left
                  << std::setw(32) << "Device Name" << "  "
                  << std::setw(18) << "Category" << "  "
                  << std::setw(28) << "Driver Type" << "  "
                  << std::setw(20) << "Service" << "  "
                  << std::setw(20) << "Upper Filters" << "  "
                  << std::setw(20) << "Lower Filters";
        if (runTest) std::cout << "  " << std::setw(12) << "Health";
        std::cout << "\n";

        for (const auto& rec : records) {
            std::cout << std::left
                      << std::setw(32) << rec.deviceName.substr(0, 31) << "  "
                      << std::setw(18) << rec.category.substr(0, 17) << "  "
                      << std::setw(28) << rec.driverType.substr(0, 27) << "  "
                      << std::setw(20) << (rec.serviceName.empty() ? "-" : ToUtf8(rec.serviceName)).substr(0, 19) << "  "
                      << std::setw(20) << join(rec.upperFilters).substr(0, 19) << "  "
                      << std::setw(20) << join(rec.lowerFilters).substr(0, 19);
            if (runTest) std::cout << "  " << std::setw(12) << HealthToString(rec.health);
            std::cout << "\n";
        }
    }

    // Renders records as a JSON array on stdout.
    static void RenderJSON(const std::vector<DeviceRecord>& records, bool runTest) {
        std::cout << "[\n";
        for (size_t i = 0; i < records.size(); ++i) {
            const auto& rec = records[i];
            std::cout << "  {\n";
            std::cout << "    \"deviceName\": \"" << EscapeJSON(rec.deviceName) << "\",\n";
            std::cout << "    \"category\": \"" << EscapeJSON(rec.category) << "\",\n";
            std::cout << "    \"driverType\": \"" << EscapeJSON(rec.driverType) << "\",\n";
            std::cout << "    \"driverProvider\": \"" << EscapeJSON(rec.driverProvider) << "\",\n";
            std::cout << "    \"driverVersion\": \"" << EscapeJSON(rec.driverVersion) << "\",\n";
            std::cout << "    \"serviceName\": \"" << EscapeJSON(ToUtf8(rec.serviceName)) << "\",\n";

            auto jsonList = [](const std::vector<std::string>& v) {
                std::string s = "[";
                for (size_t k = 0; k < v.size(); ++k) {
                    if (k > 0) s += ", ";
                    s += "\"" + EscapeJSON(v[k]) + "\"";
                }
                s += "]";
                return s;
            };
            std::cout << "    \"upperFilters\": " << jsonList(rec.upperFilters) << ",\n";
            std::cout << "    \"lowerFilters\": " << jsonList(rec.lowerFilters);

            if (runTest) {
                std::cout << ",\n";
                std::cout << "    \"health\": \"" << HealthToString(rec.health) << "\",\n";
                std::cout << "    \"problemCode\": " << rec.problemCode << ",\n";
                std::cout << "    \"diagnosticDetails\": \"" << EscapeJSON(rec.diagnosticDetails) << "\"\n";
            } else {
                std::cout << "\n";
            }
            std::cout << "  }" << (i + 1 < records.size() ? "," : "") << "\n";
        }
        std::cout << "]\n";
    }

    // Renders records as CSV on stdout (header + one row per device).
    static void RenderCSV(const std::vector<DeviceRecord>& records, bool runTest) {
        std::cout << "DeviceName,Category,DriverType,DriverProvider,DriverVersion,ServiceName,UpperFilters,LowerFilters";
        if (runTest) std::cout << ",Health,ProblemCode,DiagnosticDetails";
        std::cout << "\n";

        for (const auto& rec : records) {
            // Multi-value filter lists are joined with ';' inside a quoted cell.
            auto joinSemi = [](const std::vector<std::string>& v) {
                std::string s;
                for (size_t k = 0; k < v.size(); ++k) { if (k > 0) s += ";"; s += v[k]; }
                return s;
            };
            std::cout << EscapeCSV(rec.deviceName) << ","
                      << EscapeCSV(rec.category) << ","
                      << EscapeCSV(rec.driverType) << ","
                      << EscapeCSV(rec.driverProvider) << ","
                      << EscapeCSV(rec.driverVersion) << ","
                      << EscapeCSV(ToUtf8(rec.serviceName)) << ","
                      << EscapeCSV(joinSemi(rec.upperFilters)) << ","
                      << EscapeCSV(joinSemi(rec.lowerFilters));
            if (runTest) {
                std::cout << "," << EscapeCSV(HealthToString(rec.health))
                          << "," << rec.problemCode
                          << "," << EscapeCSV(rec.diagnosticDetails);
            }
            std::cout << "\n";
        }
    }

    // True when stdin is redirected (piped), so we can read category filters.
    static bool IsInputPiped() {
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if (h == nullptr || h == INVALID_HANDLE_VALUE) return false;
        return GetFileType(h) != FILE_TYPE_CHAR; // not a console => pipe/file
    }

    // Reads only the first whitespace-delimited token from stdin. We never use
    // more than one category filter, so avoid slurping a large piped stream into
    // a vector just to keep the first element.
    static std::string ReadPipedFirstFilter() {
        std::string token;
        if (std::cin >> token) return token;
        return "";
    }

public:
    void ShowHelp() {
        std::cout << R"(driverctl(1)               CrossShell for UNIX Reference Manual               driverctl(1)

    NAME
        driverctl - inspect and diagnose Windows PnP device drivers

    SYNOPSIS
        driverctl [COMMAND] [OPTIONS]

    DESCRIPTION
        Interfaces directly with the Windows PnP Configuration Manager to inspect
        device drivers, diagnose hardware malfunctions, start errors (Code 10),
        critical failures (Code 43), and uninstalled or corrupted states.

        When stdin is piped, driverctl reads additional category filters from stdin.

    OPTIONS
        --list
            Enumerate all installed devices, categories, and drivers.
        --test
            Run live hardware and driver malfunction diagnostics.
        --category NAME
            Filter devices by setup class/category (e.g., Display, Net, USB).
        --errors-only
            Suppress healthy devices; only report warnings and failures.
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
            Inspect all display/graphics drivers and test for malfunctions.

        driverctl --test --errors-only
            Run full diagnostic and only display failed devices.

        driverctl --list --json
            Export device inventory formatted as JSON.

    CrossShell for UNIX                                                    driverctl(1)
)";
    }

    int Run(int argc, char* argv[]) {
        bool runTest = false;
        bool listOnly = false;
        bool errorsOnly = false;
        OutputFormat format = OutputFormat::Report;
        std::string categoryFilter;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                ShowHelp();
                return 0;
            } else if (arg == "--test") {
                runTest = true;
            } else if (arg == "--list") {
                listOnly = true;
            } else if (arg == "--errors-only") {
                errorsOnly = true;
            } else if (arg == "--table") {
                format = OutputFormat::Table;
            } else if (arg == "--json") {
                format = OutputFormat::Json;
            } else if (arg == "--csv") {
                format = OutputFormat::Csv;
            } else if (arg == "--category" && i + 1 < argc) {
                categoryFilter = argv[++i];
            }
        }

        // Pipeline support: when stdin is piped, take the first token as a
        // category filter. An explicit --category always wins.
        if (categoryFilter.empty() && IsInputPiped()) {
            categoryFilter = ReadPipedFirstFilter();
        }

        if (!runTest && !listOnly) {
            ShowHelp();
            return 0;
        }

        // Structured formats render clean data to stdout and skip the report/log
        // banner so downstream tools receive parseable output only.
        if (format != OutputFormat::Report) {
            auto records = scanner.Scan(runTest, categoryFilter);
            if (errorsOnly) {
                records.erase(std::remove_if(records.begin(), records.end(),
                    [](const DeviceRecord& r) { return r.health == HealthStatus::Healthy; }),
                    records.end());
            }
            switch (format) {
                case OutputFormat::Table: RenderTable(records, runTest); break;
                case OutputFormat::Json:  RenderJSON(records, runTest);  break;
                case OutputFormat::Csv:   RenderCSV(records, runTest);   break;
                default: break;
            }
            return 0;
        }

        // Human-readable report path: only now do we construct the logger, which
        // performs disk I/O (creates ~/driverctl-logs and opens the log file).
        logger = std::make_unique<ReportLogger>();

        logger->WriteLine("");
        logger->WriteLine("                      Driverctl Device Diagnostic Report                      ");
        logger->WriteLine("");
        if (!IsRunningElevated()) {
            logger->WriteLine("NOTE: Not running as administrator - some device status and problem");
            logger->WriteLine("      codes may be incomplete. Re-run elevated for full diagnostics.");
            logger->WriteLine("");
        }
        logger->WriteLine(Fmt("Log Path: {}", logger->GetPath().string()));
        if (!categoryFilter.empty()) logger->WriteLine(Fmt("Filter: Category matching '{}'", categoryFilter));
        logger->WriteLine("\n");

        auto records = scanner.Scan(runTest, categoryFilter);

        size_t healthyCount = 0, warningCount = 0, errorCount = 0;

        for (const auto& rec : records) {
            if (rec.health == HealthStatus::Healthy) healthyCount++;
            else if (rec.health == HealthStatus::Warning) warningCount++;
            else if (rec.health == HealthStatus::Malfunction) errorCount++;

            if (errorsOnly && rec.health == HealthStatus::Healthy) continue;

            logger->WriteLine(Fmt("Device Name     : {}", rec.deviceName));
            logger->WriteLine(Fmt("Category        : {}", rec.category));
            logger->WriteLine(Fmt("Driver Type     : {}", rec.driverType));
            logger->WriteLine(Fmt("Driver Provider : {}", rec.driverProvider.empty() ? "N/A" : rec.driverProvider));
            logger->WriteLine(Fmt("Driver Version  : {}", rec.driverVersion.empty() ? "N/A" : rec.driverVersion));
            logger->WriteLine(Fmt("Service Name    : {}", rec.serviceName.empty() ? "None (Direct/Bus)" : ToUtf8(rec.serviceName)));

            if (!rec.upperFilters.empty()) {
                logger->WriteLine(Fmt("Upper Filters   : {}", JoinList(rec.upperFilters)));
            }
            if (!rec.lowerFilters.empty()) {
                logger->WriteLine(Fmt("Lower Filters   : {}", JoinList(rec.lowerFilters)));
            }

            if (runTest) {
                std::string statusTag;
                switch (rec.health) {
                    case HealthStatus::Healthy:     statusTag = "[PASS] HEALTHY"; break;
                    case HealthStatus::Warning:     statusTag = "[WARN] WARNING"; break;
                    case HealthStatus::Malfunction: statusTag = "[FAIL] MALFUNCTION DETECTED"; break;
                    case HealthStatus::Disabled:    statusTag = "[INFO] DISABLED"; break;
                }
                logger->WriteLine(Fmt("Test Verdict    : {}", statusTag));
                logger->WriteLine(Fmt("Diagnostic Note : {}", rec.diagnosticDetails));
            }
            logger->WriteLine("-------------------------------------------------------------------------------");
        }

        logger->WriteLine("\n");
        logger->WriteLine("                               Diagnostic Summary                            ");
        logger->WriteLine("");
        logger->WriteLine(Fmt("Total Devices Scanned : {}", records.size()));
        logger->WriteLine(Fmt("Operational / Healthy : {}", healthyCount));
        logger->WriteLine(Fmt("Warnings / Deferred   : {}", warningCount));
        logger->WriteLine(Fmt("Malfunctions / Errors : {}", errorCount));
        logger->WriteLine("\n");
        logger->WriteLine(Fmt("Complete report saved to: {}", logger->GetPath().string()));

        return 0;
    }
};
