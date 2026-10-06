#include "admin_check.hpp"
#include "config_parser.hpp"
#include "event_ring_buffer.hpp"
#include "ipc_action_parser.hpp"
#include "ipc_command_parser.hpp"
#include "ipc_endpoint.hpp"
#include "ipc_framing.hpp"
#include "ipc_request.hpp"
#include "ipc_response.hpp"
#include "logger.hpp"
#include "managed_process.hpp"
#include "metrics_snapshot.hpp"
#include "path_encoding.hpp"
#include "pipe_client_security.hpp"
#include "process_state.hpp"
#include "program_config_comparison.hpp"
#include "program_config.hpp"
#include "scoped_handle.hpp"
#include "service_events.hpp"
#include "status_snapshot.hpp"
#include "supervisor_defaults.hpp"
#include "supervisor.hpp"

bool Supervisord::LoadConfiguration(const std::wstring& configFile) {
        auto parse = ParseConfigDetailed(configFile);
        for (const auto& warning : parse.warnings) {
            Logger::Log("supervisord", "CONFIG WARNING: " + warning);
        }
        for (const auto& error : parse.errors) {
            Logger::Log("supervisord", "CONFIG ERROR: " + error);
            WriteServiceEvent("CONFIG ERROR: " + error, EVENTLOG_ERROR_TYPE);
        }
        if (!parse.errors.empty()) {
            return false;
        }

        config_path_ = configFile;

        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        processes_.clear();
        {
            std::lock_guard<std::mutex> lock(process_lock_map_mtx_);
            process_locks_.clear();
        }

        for (const auto& cfg : parse.configs) {
            processes_[cfg.name] = std::make_unique<ManagedProcess>(cfg);
        }
        Logger::Log("supervisord", "Loaded " + std::to_string(processes_.size()) + " configurations.");
        return true;
    }

bool Supervisord::ReloadConfiguration(std::string& summary) {
        if (config_path_.empty()) {
            summary = "no config path set";
            return false;
        }

        auto parse = ParseConfigDetailed(config_path_);
        for (const auto& warning : parse.warnings) {
            Logger::Log("supervisord", "CONFIG WARNING: " + warning);
        }
        if (!parse.errors.empty()) {
            for (const auto& error : parse.errors) Logger::Log("supervisord", "CONFIG ERROR: " + error);
            summary = "reload aborted due to config errors";
            return false;
        }

        std::map<std::wstring, ProgramConfig> nextByName;
        for (const auto& cfg : parse.configs) nextByName[cfg.name] = cfg;

        int added = 0, updated = 0, removed = 0;
        std::lock_guard<std::mutex> lock(supervisor_mtx_);

        std::vector<std::wstring> toRemove;
        for (auto& [name, proc] : processes_) {
            if (nextByName.count(name) == 0) toRemove.push_back(name);
        }
        for (const auto& name : toRemove) {
            processes_[name]->Stop();
            processes_.erase(name);
            std::lock_guard<std::mutex> lk(process_lock_map_mtx_);
            process_locks_.erase(name);
            removed++;
        }

        for (auto& [name, cfg] : nextByName) {
            auto it = processes_.find(name);
            if (it == processes_.end()) {
                processes_[name] = std::make_unique<ManagedProcess>(cfg);
                added++;
                if (cfg.autostart) processes_[name]->Start();
                continue;
            }

            if (!ProgramConfigEquals(it->second->config, cfg)) {
                it->second->Stop();
                it->second = std::make_unique<ManagedProcess>(cfg);
                updated++;
                if (cfg.autostart) it->second->Start();
            }
        }

        g_ReloadCount++;
        summary = "reload applied: added=" + std::to_string(added) + " updated=" + std::to_string(updated) + " removed=" + std::to_string(removed);
        Logger::Log("supervisord", summary);
        return true;
    }

std::vector<std::wstring> Supervisord::BuildDependencyOrderLocked() {
        std::vector<std::wstring> names;
        for (auto& [name, _] : processes_) names.push_back(name);
        std::sort(names.begin(), names.end());

        std::map<std::wstring, int> indegree;
        std::map<std::wstring, std::vector<std::wstring>> adj;
        for (const auto& n : names) indegree[n] = 0;

        for (const auto& [name, proc] : processes_) {
            for (const auto& dep : proc->config.depends_on) {
                if (indegree.count(dep)) {
                    adj[dep].push_back(name);
                    indegree[name]++;
                }
            }
        }

        std::vector<std::wstring> ready;
        for (const auto& [n, d] : indegree) if (d == 0) ready.push_back(n);
        std::sort(ready.begin(), ready.end());

        std::vector<std::wstring> out;
        while (!ready.empty()) {
            std::wstring cur = ready.front();
            ready.erase(ready.begin());
            out.push_back(cur);
            auto itAdj = adj.find(cur);
            if (itAdj == adj.end()) continue;
            for (const auto& nxt : itAdj->second) {
                indegree[nxt]--;
                if (indegree[nxt] == 0) {
                    ready.push_back(nxt);
                    std::sort(ready.begin(), ready.end());
                }
            }
        }

        if (out.size() != names.size()) return names; // cycle fallback
        return out;
    }

void Supervisord::StartAll() {
        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        auto order = BuildDependencyOrderLocked();
        for (const auto& name : order) {
            auto it = processes_.find(name);
            if (it != processes_.end() && it->second->config.autostart) it->second->Start();
        }
    }

void Supervisord::Shutdown() {
        Logger::Log("supervisord", "Stopping all managed processes...");
        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        auto order = BuildDependencyOrderLocked();
        std::reverse(order.begin(), order.end());
        std::stable_sort(order.begin(), order.end(), [&](const std::wstring& a, const std::wstring& b) {
            return processes_[a]->config.shutdown_phase > processes_[b]->config.shutdown_phase;
        });
        for (const auto& name : order) {
            auto it = processes_.find(name);
            if (it != processes_.end()) it->second->Stop();
        }
        Logger::Log("supervisord", "Shutdown complete.");
    }

MetricsSnapshot Supervisord::BuildMetricsSnapshot() {
        MetricsSnapshot snapshot;
        auto now = std::chrono::steady_clock::now();
        snapshot.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        snapshot.ipc_requests = g_IpcRequests.load();
        snapshot.ipc_errors = g_IpcErrors.load();
        snapshot.reload_count = g_ReloadCount.load();
        snapshot.process_starts = ManagedProcess::g_totalStarts.load();
        snapshot.launch_failures = ManagedProcess::g_totalLaunchFailures.load();
        snapshot.unexpected_exits = ManagedProcess::g_totalUnexpectedExits.load();
        snapshot.restarts = ManagedProcess::g_totalRestarts.load();
        snapshot.healthcheck_failures = ManagedProcess::g_totalHealthCheckFailures.load();
        return snapshot;
    }

std::string Supervisord::RenderMetricsJson(const MetricsSnapshot& snapshot) const {
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"uptime_seconds\": " << snapshot.uptime_seconds << ",\n";
        ss << "  \"ipc_requests\": " << snapshot.ipc_requests << ",\n";
        ss << "  \"ipc_errors\": " << snapshot.ipc_errors << ",\n";
        ss << "  \"reload_count\": " << snapshot.reload_count << ",\n";
        ss << "  \"process_starts\": " << snapshot.process_starts << ",\n";
        ss << "  \"launch_failures\": " << snapshot.launch_failures << ",\n";
        ss << "  \"unexpected_exits\": " << snapshot.unexpected_exits << ",\n";
        ss << "  \"restarts\": " << snapshot.restarts << ",\n";
        ss << "  \"healthcheck_failures\": " << snapshot.healthcheck_failures << "\n";
        ss << "}\n";
        return ss.str();
    }

std::vector<StatusSnapshotEntry> Supervisord::BuildStatusSnapshot() {
        std::vector<StatusSnapshotEntry> snapshot;
        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        snapshot.reserve(processes_.size());
        for (auto& [name, proc] : processes_) {
            snapshot.push_back({WideToUtf8(name), StateToString(proc->GetState())});
        }
        return snapshot;
    }

std::string Supervisord::RenderStatusSnapshot(const std::vector<StatusSnapshotEntry>& snapshot) const {
        std::stringstream res;
        for (const auto& entry : snapshot) {
            res << std::left << std::setw(25) << entry.name << std::setw(15) << entry.state << "\n";
        }
        return res.str();
    }

std::string Supervisord::BuildMetricsJson() {
        return RenderMetricsJson(BuildMetricsSnapshot());
    }

void Supervisord::WriteMetricsFile() {
        std::wstring path = GetExecutableDir() + L"\\supervisord.metrics.json";
        std::ofstream out(path, std::ios::trunc | std::ios::binary);
        if (!out.is_open()) return;
        std::string content = BuildMetricsJson();
        out.write(content.data(), content.size());
    }

std::mutex& Supervisord::GetProcessCommandLock(const std::wstring& processName) {
        std::lock_guard<std::mutex> lock(process_lock_map_mtx_);
        auto it = process_locks_.find(processName);
        if (it == process_locks_.end()) {
            auto inserted = process_locks_.emplace(processName, std::make_unique<std::mutex>());
            return *(inserted.first->second);
        }
        return *(it->second);
    }

bool Supervisord::ExecuteWithProcessLocked(const std::wstring& processName, const std::function<void(ManagedProcess&)>& fn) {
        std::lock_guard<std::mutex> processGuard(GetProcessCommandLock(processName));
        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        auto it = processes_.find(processName);
        if (it == processes_.end()) return false;
        fn(*it->second);
        return true;
    }

std::string Supervisord::HandleIpcCommand(const std::string& cmd, bool clientPrivileged) {
        g_IpcRequests++;
        IpcRequest req;
        req.raw = cmd;
        req.clientPrivileged = clientPrivileged;
        req.tokens = ParseIpcCommandTokens(cmd);
        if (req.tokens.empty()) {
            g_IpcErrors++;
            return IpcResponse::Err(IpcCodes::kErrInvalidSyntax, "empty command").ToFrame();
        }

        req.actionText = req.tokens[0];
        req.action = ParseIpcAction(req.actionText);
        if (req.tokens.size() > 1) {
            req.target = req.tokens[1];
            req.wTarget = Utf8ToWide(req.target);
        }

        auto requirePrivileged = [&req]() -> bool {
            return req.clientPrivileged || IsCurrentProcessElevatedOrAdmin();
        };

        std::map<IpcAction, std::function<IpcResponse()>> handlers;
        handlers[IpcAction::Status] = [&]() {
            return IpcResponse::OkWithPayload(IpcCodes::kOkStatus, RenderStatusSnapshot(BuildStatusSnapshot()));
        };
        handlers[IpcAction::Diag] = [&]() {
            size_t limit = SupervisorDefaults::kDefaultDiagLimit;
            if (req.tokens.size() > 1) {
                try { limit = static_cast<size_t>(std::stoul(req.tokens[1])); } catch (...) { limit = SupervisorDefaults::kDefaultDiagLimit; }
            }
            return IpcResponse::OkWithPayload(IpcCodes::kOkDiag, EventRingBuffer::Dump(limit));
        };
        handlers[IpcAction::Metrics] = [&]() {
            return IpcResponse::OkWithPayload(IpcCodes::kOkMetrics, BuildMetricsJson());
        };
        handlers[IpcAction::Reload] = [&]() {
            if (!requirePrivileged()) return IpcResponse::Err(IpcCodes::kErrAccessDenied, "reload requires admin privileges");
            std::string summary;
            if (ReloadConfiguration(summary)) return IpcResponse::Ok(IpcCodes::kOkReloaded, summary);
            return IpcResponse::Err(IpcCodes::kErrReloadFailed, summary);
        };
        handlers[IpcAction::Start] = [&]() {
            if (!requirePrivileged()) return IpcResponse::Err(IpcCodes::kErrAccessDenied, "start requires admin privileges");
            if (req.target.empty()) return IpcResponse::Err(IpcCodes::kErrMissingTarget, "start requires process_name");
            if (!ExecuteWithProcessLocked(req.wTarget, [](ManagedProcess& p) { p.Start(); })) {
                return IpcResponse::Err(IpcCodes::kErrNoSuchProcess, req.target);
            }
            return IpcResponse::Ok(IpcCodes::kOkStarted, req.target);
        };
        handlers[IpcAction::Stop] = [&]() {
            if (!requirePrivileged()) return IpcResponse::Err(IpcCodes::kErrAccessDenied, "stop requires admin privileges");
            if (req.target.empty()) return IpcResponse::Err(IpcCodes::kErrMissingTarget, "stop requires process_name");
            if (!ExecuteWithProcessLocked(req.wTarget, [](ManagedProcess& p) { p.Stop(); })) {
                return IpcResponse::Err(IpcCodes::kErrNoSuchProcess, req.target);
            }
            return IpcResponse::Ok(IpcCodes::kOkStopped, req.target);
        };
        handlers[IpcAction::Restart] = [&]() {
            if (!requirePrivileged()) return IpcResponse::Err(IpcCodes::kErrAccessDenied, "restart requires admin privileges");
            if (req.target.empty()) return IpcResponse::Err(IpcCodes::kErrMissingTarget, "restart requires process_name");
            if (!ExecuteWithProcessLocked(req.wTarget, [](ManagedProcess& p) { p.Stop(); p.Start(); })) {
                return IpcResponse::Err(IpcCodes::kErrNoSuchProcess, req.target);
            }
            return IpcResponse::Ok(IpcCodes::kOkRestarted, req.target);
        };

        auto it = handlers.find(req.action);
        IpcResponse response;
        if (it == handlers.end()) {
            response = IpcResponse::Err(IpcCodes::kErrUnknownCommand, req.actionText);
        } else {
            response = it->second();
        }
        if (!response.ok) g_IpcErrors++;
        return response.ToFrame();
    }

void Supervisord::RunIpcServer() {
        ipc_thread_ = std::thread([this]() {
            PSECURITY_DESCRIPTOR pSD = NULL;
            // SDDL: Allow access to Local System (SY), Built-in Administrators (BA), and Authenticated Users (AU)
            ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;AU)", SDDL_REVISION_1, &pSD, NULL);

            SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), pSD, FALSE };

            while (!g_SignalReceived) {
                ScopedHandle hPipe(CreateNamedPipeW(
                    PIPE_NAME, PIPE_ACCESS_DUPLEX,
                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                    1, SupervisorDefaults::kPipeOutBuffer, SupervisorDefaults::kPipeInBuffer, 0, &sa
                ));

                if (!hPipe.isValid()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }

                if (ConnectNamedPipe(hPipe.get(), NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
                    if (g_SignalReceived) break;
                    std::string request;
                    if (ReadIpcFrame(hPipe.get(), request)) {
                        bool privileged = IsPipeClientPrivileged(hPipe.get());
                        std::string reply = HandleIpcCommand(request, privileged);
                        WriteIpcFrame(hPipe.get(), reply);
                    } else {
                        WriteIpcFrame(hPipe.get(), IpcResponse::Err(IpcCodes::kErrBadFrame, "invalid IPC frame").ToFrame());
                    }
                    FlushFileBuffers(hPipe.get());
                }
                DisconnectNamedPipe(hPipe.get());
            }

            if (pSD) LocalFree(pSD);
        });
    }

void Supervisord::JoinIpcServer() {
        g_SignalReceived = true;
        // Unblock ConnectNamedPipe by pinging the pipe locally
        HANDLE hPipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);

        if (ipc_thread_.joinable()) ipc_thread_.join();
    }
