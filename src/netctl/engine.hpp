#ifndef NETCTL_ENGINE_HPP
#define NETCTL_ENGINE_HPP

#include "netctl.hpp"

class PcapWriter {
public:
    PcapWriter() = default;
    explicit PcapWriter(const std::string& filename);

    bool open(const std::string& filename, uint32_t network);
    bool is_open() const;
    void write_packet(const uint8_t* data, size_t length, const std::chrono::system_clock::time_point& tp);

private:
    std::ofstream file_;
    mutable std::mutex mutex_;
};

class PacketQueue {
public:
    void push(RawPacket&& packet);
    bool pop(RawPacket& packet, const std::atomic<bool>& running);
    void notify_all();
    uint64_t get_dropped_count() const;

private:
    std::queue<RawPacket> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t max_capacity_ = 50000;
    std::atomic<uint64_t> dropped_count_{0};
};

class Logger {
public:
    Logger(const std::string& filepath, bool disable_color);
    ~Logger();

    void log(const std::string& colorized_msg, const std::string& plain_msg);

private:
    std::ofstream file_stream_;
    std::mutex log_mutex_;
    bool logging_to_file_ = false;
    bool use_color_ = true;
    uint64_t write_count_ = 0;
};

// System & Utility Functions
bool IsUserAdmin();
void EnableVT100Colors();
BOOL WINAPI ConsoleHandler(DWORD signal);
void RequestStop();

std::vector<std::pair<std::string, std::string>> ListNetworkInterfaces();
void StartCaptureEngine(const Config& config, Logger& logger);
void StartNpcapCapture(const Config& config, Logger& logger);

#endif // NETCTL_ENGINE_HPP
