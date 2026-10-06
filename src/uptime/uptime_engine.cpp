#include "system_metrics_sampler.hpp"
#include "uptime_calculator.hpp"
#include "uptime_engine.hpp"
#include "uptime_options.hpp"

UptimeEngine::UptimeEngine(UptimeOptions opts) : options(std::move(opts)) {}

int UptimeEngine::execute() {
        if (options.since) {
            std::cout << "system up since " << UptimeCalculator::getBootTimeString() << "\n";
            return 0;
        }

        std::string timeStr = UptimeCalculator::getCurrentTimeFormatted();
        std::string uptimeStr = options.pretty ? UptimeCalculator::getPrettyUptimeString() : UptimeCalculator::getUptimeString();
        int userCount = SystemMetricsSampler::getActiveUserCount();
        double cpuLoad = SystemMetricsSampler::getCpuUtilization();

        std::cout << " " << timeStr << "  "
                  << uptimeStr << ",  "
                  << userCount << (userCount == 1 ? " user,  " : " users,  ")
                  << "load averages: "
                  << std::fixed << std::setprecision(2) << cpuLoad << ", "
                  << cpuLoad << ", "
                  << cpuLoad << "\n";

        return 0;
    }
