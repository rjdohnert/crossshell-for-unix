#include "sleep_engine.hpp"
#include "duration_parser.hpp"
#include "telemetry_reporter.hpp"

int SleepEngine::execute(const SleepOptions& options) {
    if (options.raw_delays.empty() && !options.has_explicit) {
        std::wcerr << L"usage: sleep NUMBER[SUFFIX]... or sleep [OPTIONS]\n"
                   << L"Try 'sleep --help' for more information.\n";
        return 1;
    }

    double total_sleep_ms = options.explicit_seconds * 1000.0;

    for (const auto& delay : options.raw_delays) {
        if (!DurationParser::parseSleepArgument(delay, total_sleep_ms)) {
            return 1;
        }
    }

    if (total_sleep_ms < 0) total_sleep_ms = 0;

    if (options.verbose && !options.quiet) {
        std::wcout << L"sleep: sleeping for " << (total_sleep_ms / 1000.0) << L" seconds ("
                   << static_cast<long long>(total_sleep_ms) << L" ms)...\n";
    }

    long long ms_to_sleep = static_cast<long long>(total_sleep_ms);
    if (ms_to_sleep > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms_to_sleep));
    }

    TelemetryReporter::report(options, ms_to_sleep, total_sleep_ms);
    return 0;
}
