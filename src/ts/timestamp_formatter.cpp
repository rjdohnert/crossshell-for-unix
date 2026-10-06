#include "timestamp_formatter.hpp"
#include "ts_options.hpp"

TimestampFormatter::TimestampFormatter(TsOptions opts)
        : options(std::move(opts)),
          startTime(std::chrono::high_resolution_clock::now()),
          lastTime(startTime) {}

std::string TimestampFormatter::generatePrefix() {
        auto now = std::chrono::high_resolution_clock::now();

        if (options.mode == TsMode::ElapsedSinceStart) {
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime);
            return formatDuration(elapsed.count());
        } else if (options.mode == TsMode::Incremental) {
            auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime);
            lastTime = now;
            return formatDuration(delta.count());
        }

        // WallClock Mode
        auto sysNow = std::chrono::system_clock::now();
        auto sysTime = std::chrono::system_clock::to_time_t(sysNow);
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(sysNow.time_since_epoch()).count() % 1000000000;

        std::tm tmVal{};
        if (options.useUtc) {
            gmtime_s(&tmVal, &sysTime);
        } else {
            localtime_s(&tmVal, &sysTime);
        }

        if (options.isIso) {
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmVal);
            std::ostringstream oss;
            oss << buf;
            if (options.subsecondPrecision > 0) {
                oss << "." << formatSubseconds(nanos, options.subsecondPrecision);
            }
            oss << (options.useUtc ? "Z" : "");
            return oss.str();
        }

        if (!options.customFormat.empty()) {
            char buf[256];
            std::strftime(buf, sizeof(buf), options.customFormat.c_str(), &tmVal);
            return std::string(buf);
        }

        char buf[64];
        std::strftime(buf, sizeof(buf), "%b %d %H:%M:%S", &tmVal);
        std::ostringstream oss;
        oss << buf;
        int prec = (options.subsecondPrecision >= 0) ? options.subsecondPrecision : 6;
        if (prec > 0) {
            oss << "." << formatSubseconds(nanos, prec);
        }
        return oss.str();
    }

std::string TimestampFormatter::formatSubseconds(int64_t nanos, int precision) {
        std::ostringstream oss;
        oss << std::setw(9) << std::setfill('0') << nanos;
        std::string s = oss.str();
        return s.substr(0, std::min<size_t>(precision, 9));
    }

std::string TimestampFormatter::formatDuration(int64_t totalNanos) const {
        int64_t totalSec = totalNanos / 1000000000;
        int64_t remNanos = totalNanos % 1000000000;

        int64_t days = totalSec / 86400;
        int64_t hours = (totalSec % 86400) / 3600;
        int64_t mins = (totalSec % 3600) / 60;
        int64_t secs = totalSec % 60;

        std::ostringstream oss;
        if (days > 0) {
            oss << days << "d ";
        }
        if (hours > 0 || days > 0) {
            oss << std::setw(2) << std::setfill('0') << hours << ":";
        }
        oss << std::setw(2) << std::setfill('0') << mins << ":"
            << std::setw(2) << std::setfill('0') << secs;

        int prec = (options.subsecondPrecision >= 0) ? options.subsecondPrecision : 6;
        if (prec > 0) {
            oss << "." << formatSubseconds(remNanos, prec);
        }
        return oss.str();
    }
