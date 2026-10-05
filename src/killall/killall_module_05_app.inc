// ============================================================================
// Application Controller & Execution Engine
// ============================================================================
class KillallApp {
private:
    Options options;
    SignalRegistry registry;
    ProcessSignaler signaler;

    bool askConfirmation(const std::string& name, DWORD pid) {
        // Read directly from console (CONIN$) to allow interactive prompts during piping
        FILE* conin = nullptr;
        if (fopen_s(&conin, "CONIN$", "r") != 0 || !conin) {
            std::cout << "Kill " << name << "(" << pid << ")? (y/n): ";
            std::string ans;
            std::cin >> ans;
            return (!ans.empty() && (ans[0] == 'y' || ans[0] == 'Y'));
        }

        std::cerr << "Kill " << name << "(" << pid << ")? (y/n): ";
        char ch = 0;
        int r = fgetc(conin);
        fclose(conin);
        return (r == 'y' || r == 'Y');
    }

    bool matchesPattern(const std::string& procName, const std::string& pattern) {
        if (options.useRegex) {
            try {
                auto flags = std::regex::ECMAScript;
                if (options.ignoreCase) flags |= std::regex::icase;
                std::regex re(pattern, flags);
                return std::regex_search(procName, re);
            } catch (const std::regex_error&) {
                return false;
            }
        }

        std::string pName = procName;
        std::string pat = pattern;

        if (options.ignoreCase) {
            std::transform(pName.begin(), pName.end(), pName.begin(), ::tolower);
            std::transform(pat.begin(), pat.end(), pat.begin(), ::tolower);
        }

        if (pName == pat) return true;
        if (pName == pat + ".exe") return true;
        return false;
    }

    void collectTreeChildren(DWORD parentPid, const std::vector<ProcessMeta>& snapshot, std::set<DWORD>& treePids) {
        for (const auto& proc : snapshot) {
            if (proc.parentPid == parentPid && proc.pid != parentPid) {
                if (treePids.insert(proc.pid).second) {
                    collectTreeChildren(proc.pid, snapshot, treePids);
                }
            }
        }
    }

    std::vector<std::string> gatherPatterns() {
        std::vector<std::string> patterns = options.patterns;
        bool hasStdInPipe = !_isatty(_fileno(stdin));

        if (patterns.empty() || (patterns.size() == 1 && patterns[0] == "-") || hasStdInPipe) {
            if (hasStdInPipe || (patterns.size() == 1 && patterns[0] == "-")) {
                if (!patterns.empty() && patterns[0] == "-") patterns.clear();
                std::string token;
                while (std::cin >> token) {
                    patterns.push_back(token);
                }
            }
        }
        return patterns;
    }

public:
    explicit KillallApp(Options opt) : options(std::move(opt)) {}

    int run() {
        if (options.listSignals) {
            for (const auto& sig : registry.getAll()) {
                std::cout << std::setw(2) << sig.number << ") " << std::setw(8) << std::left << sig.name;
                if (sig.number % 4 == 0) std::cout << "\n";
            }
            std::cout << "\n";
            return 0;
        }

        if (options.listTable) {
            std::cout << std::left << std::setw(6) << "NUM" << std::setw(12) << "NAME" << "DESCRIPTION\n";
            std::cout << std::string(75, '-') << "\n";
            for (const auto& sig : registry.getAll()) {
                std::cout << std::setw(6) << sig.number << std::setw(12) << sig.name << sig.description << "\n";
            }
            return 0;
        }

        auto patterns = gatherPatterns();
        if (patterns.empty()) {
            if (!options.quiet) {
                std::cerr << "killall: No process name patterns specified.\n";
                std::cerr << "         Use 'killall --help' for usage instructions.\n";
            }
            return 1;
        }

        PrivilegeManager::enableDebugPrivilege();
        auto snapshot = ProcessQueryEngine::collectProcesses();

        std::map<DWORD, ProcessMeta> targetMap;

        for (const auto& pat : patterns) {
            bool foundAny = false;
            for (const auto& proc : snapshot) {
                if (!matchesPattern(proc.name, pat)) continue;

                // Apply owner filter
                if (!options.userFilter.empty()) {
                    std::string u1 = proc.owner, u2 = options.userFilter;
                    std::transform(u1.begin(), u1.end(), u1.begin(), ::tolower);
                    std::transform(u2.begin(), u2.end(), u2.begin(), ::tolower);
                    if (u1 != u2) continue;
                }

                // Apply age filters
                if (options.olderThan && proc.age < *options.olderThan) continue;
                if (options.youngerThan && proc.age > *options.youngerThan) continue;

                targetMap[proc.pid] = proc;
                foundAny = true;
            }

            if (!foundAny && !options.quiet) {
                std::cerr << "killall: " << pat << ": no process found\n";
            }
        }

        if (targetMap.empty()) {
            return options.quiet ? 0 : 1;
        }

        // Expand process tree if requested
        if (options.tree) {
            std::set<DWORD> treePids;
            for (const auto& [pid, _] : targetMap) {
                treePids.insert(pid);
                collectTreeChildren(pid, snapshot, treePids);
            }
            for (DWORD pid : treePids) {
                if (targetMap.find(pid) == targetMap.end()) {
                    auto it = std::find_if(snapshot.begin(), snapshot.end(), [pid](const ProcessMeta& m) {
                        return m.pid == pid;
                    });
                    if (it != snapshot.end()) targetMap[pid] = *it;
                }
            }
        }

        std::vector<HANDLE> waitHandles;
        int failureCount = 0;

        for (const auto& [pid, meta] : targetMap) {
            if (options.interactive) {
                if (!askConfirmation(meta.name, pid)) {
                    continue;
                }
            }

            if (options.wait) {
                HANDLE hProc = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
                if (hProc) waitHandles.push_back(hProc);
            }

            bool ok = signaler.sendSignal(pid, options.signal, options.force);
            if (ok) {
                if (options.verbose) {
                    std::cout << "Killed " << meta.name << "(" << pid << ") with " << options.signal.name << "\n";
                }
            } else {
                failureCount++;
                if (!options.quiet) {
                    std::cerr << "killall: Failed to signal " << meta.name << "(" << pid << ")\n";
                }
            }
        }

        // Wait for processes to exit if -w is specified
        if (options.wait && !waitHandles.empty()) {
            if (options.verbose) std::cout << "Waiting for processes to exit...\n";
            for (HANDLE h : waitHandles) {
                ::WaitForSingleObject(h, INFINITE);
                ::CloseHandle(h);
            }
        }

        return (failureCount == 0) ? 0 : 1;
    }
};

