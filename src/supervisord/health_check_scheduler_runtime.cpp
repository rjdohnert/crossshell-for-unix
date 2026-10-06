#include "health_check_scheduler_runtime.hpp"
#include "health_check_scheduler.hpp"
#include "managed_process.hpp"

namespace {
// HealthCheckSchedulerImpl public surface:
// - RegisterProcess/UnregisterProcess are safe under concurrent lifecycle operations.
// - Unregister waits until in-flight callback for that process has completed.
// - One shared worker thread schedules all health checks.
class HealthCheckSchedulerImpl {
public:
    HealthCheckSchedulerImpl() {
        worker_ = std::thread([this]() { Run(); });
    }

    ~HealthCheckSchedulerImpl() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
            cv_.notify_all();
        }
        if (worker_.joinable()) worker_.join();
    }

    void RegisterProcess(ManagedProcess* process, int intervalSeconds) {
        if (process == nullptr || intervalSeconds <= 0) return;
        std::lock_guard<std::mutex> lock(mtx_);
        Entry e;
        e.intervalSeconds = intervalSeconds;
        e.nextDue = std::chrono::steady_clock::now() + std::chrono::seconds(intervalSeconds);
        entries_[process] = e;
        cv_.notify_all();
    }

    void UnregisterProcess(ManagedProcess* process) {
        if (process == nullptr) return;
        std::unique_lock<std::mutex> lock(mtx_);
        entries_.erase(process);
        cv_.notify_all();
        cv_.wait(lock, [&]() { return inFlight_.count(process) == 0; });
    }

private:
    struct Entry {
        int intervalSeconds = 0;
        std::chrono::steady_clock::time_point nextDue;
    };

    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::thread worker_;
    std::map<ManagedProcess*, Entry> entries_;
    std::set<ManagedProcess*> inFlight_;

    void Run() {
        std::unique_lock<std::mutex> lock(mtx_);
        while (!stop_) {
            if (entries_.empty()) {
                cv_.wait(lock, [&]() { return stop_ || !entries_.empty(); });
                continue;
            }

            auto earliest = std::min_element(
                entries_.begin(),
                entries_.end(),
                [](const auto& a, const auto& b) { return a.second.nextDue < b.second.nextDue; }
            );

            auto nextDue = earliest->second.nextDue;
            cv_.wait_until(lock, nextDue, [&]() { return stop_; });
            if (stop_) break;

            auto now = std::chrono::steady_clock::now();
            std::vector<ManagedProcess*> due;
            for (auto& [proc, entry] : entries_) {
                if (entry.nextDue <= now) {
                    due.push_back(proc);
                    entry.nextDue = now + std::chrono::seconds(entry.intervalSeconds);
                    inFlight_.insert(proc);
                }
            }

            for (ManagedProcess* proc : due) {
                lock.unlock();
                proc->OnSharedHealthCheckTick();
                lock.lock();
                inFlight_.erase(proc);
                cv_.notify_all();
            }
        }
    }
};
} // namespace

HealthCheckScheduler& HealthCheckScheduler::Instance() {
    static HealthCheckScheduler instance;
    return instance;
}

static HealthCheckSchedulerImpl& GetHealthCheckSchedulerImpl() {
    static HealthCheckSchedulerImpl impl;
    return impl;
}

void HealthCheckScheduler::RegisterProcess(ManagedProcess* process, int intervalSeconds) {
    GetHealthCheckSchedulerImpl().RegisterProcess(process, intervalSeconds);
}

void HealthCheckScheduler::UnregisterProcess(ManagedProcess* process) {
    GetHealthCheckSchedulerImpl().UnregisterProcess(process);
}
