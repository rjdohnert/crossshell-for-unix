// ============================================================================
// Application Entry and Pipeline Orchestrator
// ============================================================================
class KillApplication {
private:
    Options options;
    SignalRegistry registry;
    ProcessSignaler signaler;

    void printSignalList() const {
        if (!options.targets.empty()) {
            for (const auto& t : options.targets) {
                auto sig = registry.find(t);
                if (sig) {
                    std::cout << sig->number << " " << sig->name << "\n";
                } else {
                    std::cerr << "kill: Unknown signal " << t << "\n";
                }
            }
            return;
        }

        for (const auto& sig : registry.getAll()) {
            std::cout << std::setw(2) << sig.number << ") " << std::setw(8) << std::left << sig.name;
            if (sig.number % 4 == 0) std::cout << "\n";
        }
        std::cout << "\n";
    }

    void printSignalTable() const {
        std::cout << std::left << std::setw(6) << "NUM"
                  << std::setw(12) << "NAME"
                  << "DESCRIPTION / WINDOWS ACTION\n";
        std::cout << std::string(75, '-') << "\n";
        for (const auto& sig : registry.getAll()) {
            std::cout << std::setw(6) << sig.number
                      << std::setw(12) << sig.name
                      << sig.description << "\n";
        }
    }

    std::vector<std::string> collectTargets() {
        std::vector<std::string> allTargets = options.targets;

        // If no targets were specified or '-' is passed, read stdin (piping support)
        bool hasStdInPipe = !_isatty(_fileno(stdin));
        if (allTargets.empty() || (allTargets.size() == 1 && allTargets[0] == "-") || hasStdInPipe) {
            if (hasStdInPipe || (allTargets.size() == 1 && allTargets[0] == "-")) {
                if (!allTargets.empty() && allTargets[0] == "-") {
                    allTargets.clear();
                }
                std::string token;
                while (std::cin >> token) {
                    allTargets.push_back(token);
                }
            }
        }
        return allTargets;
    }

public:
    KillApplication(Options opt) : options(std::move(opt)) {}

    int run() {
        if (options.listSignals) {
            printSignalList();
            return 0;
        }
        if (options.listTable) {
            printSignalTable();
            return 0;
        }

        auto targets = collectTargets();
        if (targets.empty()) {
            std::cerr << "kill: No target processes specified.\n";
            std::cerr << "      Use 'kill --help' for usage syntax.\n";
            return 1;
        }

        PrivilegeManager::enableDebugPrivilege();

        int failureCount = 0;
        int successCount = 0;

        for (const auto& targetStr : targets) {
            auto pids = ProcessEnumerator::resolveTarget(targetStr, options.tree);

            if (pids.empty()) {
                std::cerr << "kill: No active process matching '" << targetStr << "' found.\n";
                failureCount++;
                continue;
            }

            for (DWORD pid : pids) {
                if (pid == 0 || pid == 4) { // Guard against System Idle & System PID
                    if (options.verbose) {
                        std::cerr << "kill: Protected system process (PID " << pid << ") skipped.\n";
                    }
                    continue;
                }

                bool ok = signaler.dispatchSignal(pid, options.signal, options.force);
                if (ok) {
                    successCount++;
                    if (options.verbose) {
                        std::cout << "kill: Signal " << options.signal.name
                                  << " (" << options.signal.number << ") sent to PID " << pid << ".\n";
                    }
                } else {
                    failureCount++;
                    std::cerr << "kill: Failed to send signal " << options.signal.name
                              << " to PID " << pid << " (Error " << ::GetLastError() << ").\n";
                }
            }
        }

        return (failureCount == 0) ? 0 : 1;
    }
};

