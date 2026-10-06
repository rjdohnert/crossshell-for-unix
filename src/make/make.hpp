#ifndef MAKE_HPP
#define MAKE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <fstream>
#include <cstdint>

namespace fs = std::filesystem;

constexpr char PROG_NAME[] = "make";
constexpr char VERSION[]   = "2026.2.0";

namespace Color {
    inline const char* RESET   = "\033[0m";
    inline const char* RED     = "\033[91m";
    inline const char* GREEN   = "\033[92m";
    inline const char* YELLOW  = "\033[93m";
    inline const char* BLUE    = "\033[94m";
    inline const char* CYAN    = "\033[96m";
    inline const char* BOLD    = "\033[1m";
    inline bool enabled = true;
}

inline uint64_t hash_file_content(const fs::path& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return 0;

    uint64_t hash = 14695981039346656037ULL;
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::streamsize bytes = file.gcount();
        for (std::streamsize i = 0; i < bytes; ++i) {
            hash ^= static_cast<uint8_t>(buffer[i]);
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};

public:
    ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->cv.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(task);
        }
        cv.notify_one();
    }

    ~ThreadPool() {
        stop = true;
        cv.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};

struct Config {
    std::string makefile_path;
    std::vector<std::string> target_goals;
    std::unordered_map<std::string, std::string> cli_macros;
    unsigned int jobs = std::thread::hardware_concurrency();
    bool dry_run = false;
    bool silent = false;
    bool keep_going = false;
    bool always_build = false;
    bool hash_check = false;
    bool export_compile_commands = false;
    bool json_output = false;
    bool show_help = false;
    bool show_version = false;
    fs::path change_dir;
};

struct Rule {
    std::string target;
    std::vector<std::string> prerequisites;
    std::vector<std::string> recipes;
    bool is_phony = false;
};

class HashCache {
    std::unordered_map<std::string, uint64_t> cache;
    fs::path cache_file = ".make.hash";
    std::mutex cache_mutex;

public:
    HashCache() {
        if (fs::exists(cache_file)) {
            std::ifstream in(cache_file);
            std::string file;
            uint64_t hash;
            while (in >> file >> hash) {
                cache[file] = hash;
            }
        }
    }

    ~HashCache() {
        std::ofstream out(cache_file);
        for (const auto& [file, hash] : cache) {
            out << file << " " << hash << "\n";
        }
    }

    bool is_changed(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (!fs::exists(filepath)) return true;
        uint64_t current_hash = hash_file_content(filepath);
        auto it = cache.find(filepath);
        if (it == cache.end() || it->second != current_hash) {
            cache[filepath] = current_hash;
            return true;
        }
        return false;
    }
};

struct DAGNode {
    std::string name;
    Rule rule;
    std::atomic<int> pending_prereqs{0};
    std::vector<std::string> dependents;
    bool needs_build = false;
    bool failed = false;
};

#endif // MAKE_HPP
