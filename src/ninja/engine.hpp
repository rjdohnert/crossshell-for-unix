#ifndef NINJA_ENGINE_HPP
#define NINJA_ENGINE_HPP

#include "ninja.hpp"

namespace EnterpriseNinja {

class ProcessTracker {
public:
    static inline std::atomic<bool> g_interrupted{false};
    static inline HANDLE g_job = NULL;

    static void Initialize();
    static void Cleanup();
    static void RegisterProcess(HANDLE hProcess);
    static void UnregisterProcess(HANDLE hProcess);
    static void KillAllActive();
    static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);
    static void InstallSignalHandlers();
};

class ShardedStatCache {
public:
    static constexpr size_t NUM_SHARDS = 64;
    static uint64_t GetMtime(std::string_view path);

private:
    struct Shard {
        std::mutex lock;
        std::unordered_map<std::string_view, uint64_t> cache;
    };
    static inline std::array<Shard, NUM_SHARDS> shards_;
};

class SubprocessEngine {
public:
    static bool HasShellMetachars(std::string_view cmd);
    static int Execute(const std::string& cmd, bool dry_run, bool verbose, std::string& output_buffer);
};

class BuildEngine {
public:
    ChainedArena arena;
    std::unordered_map<std::string_view, Node*> node_map;
    std::unordered_map<std::string_view, Rule> rule_map;
    std::vector<Edge*> all_edges;
    std::vector<Node*> default_targets;

    Node* GetOrCreateNode(std::string_view path);
    void DetermineDirtyState(Node* node);
    bool ExecuteBuild(const std::vector<Node*>& targets, int parallelism, bool dry_run, bool verbose, int keep_going);
};

class ManifestParser {
public:
    static bool ParseFile(const std::string& path, BuildEngine& engine);
};

} // namespace EnterpriseNinja

#endif // NINJA_ENGINE_HPP
