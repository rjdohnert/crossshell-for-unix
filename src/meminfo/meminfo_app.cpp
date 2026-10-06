#include "meminfo_app.hpp"

int MemInfoApp::Run(int argc, char* argv[]) {
    Config cfg = CommandLineParser::Parse(argc, argv);

    if (cfg.showHelp) {
        CommandLineParser::PrintHelp();
        return 0;
    }

    if (cfg.showVersion) {
        std::cout << "meminfo version 1.0.0\n";
        return 0;
    }

    std::unique_ptr<OutputFormatter> formatter;
    switch (cfg.format) {
        case OutputFormat::Json:  formatter = std::make_unique<JsonFormatter>(); break;
        case OutputFormat::Csv:   formatter = std::make_unique<CsvFormatter>(); break;
        case OutputFormat::Table:
        default:                  formatter = std::make_unique<TableFormatter>(); break;
    }

    int iterations = 0;
    while (true) {
        MemorySnapshot snap = MemoryMetricsCollector::Collect();
        formatter->Render(std::cout, snap, cfg);
        std::cout.flush();

        iterations++;
        if (cfg.repeatSeconds <= 0 || (cfg.maxCount > 0 && iterations >= cfg.maxCount)) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(cfg.repeatSeconds));
    }

    return 0;
}
