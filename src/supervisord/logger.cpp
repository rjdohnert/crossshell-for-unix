#include "event_ring_buffer.hpp"
#include "logger.hpp"
#include "service_events.hpp"

void Logger::Log(const std::string& prefix, const std::string& message) {
        std::lock_guard<std::mutex> lock(g_logMtx);
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm;
        localtime_s(&tm, &now);
        std::stringstream line;
        line << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S] ") << "[" << prefix << "] " << message;
        std::cout << line.str() << std::endl;
        EventRingBuffer::Add(line.str());
        WriteServiceEvent("[" + prefix + "] " + message, EVENTLOG_INFORMATION_TYPE);
    }
