#include "graph_node_manager.hpp"
#include "tsort_engine.hpp"
#include "tsort_options.hpp"
#include "tsort_reporter.hpp"

TsortEngine::TsortEngine(TsortOptions opts) : options(std::move(opts)) {}

int TsortEngine::execute() {
        std::vector<std::string> allTokens;

        for (const auto& fname : options.filenames) {
            if (fname == "-") {
                std::string tok;
                while (std::cin >> tok) allTokens.push_back(tok);
            } else {
                std::ifstream f(fname);
                if (!f.is_open()) {
                    std::cerr << "tsort: cannot open " << fname << "\n";
                    return 1;
                }
                std::string tok;
                while (f >> tok) allTokens.push_back(tok);
            }
        }

        if (allTokens.size() % 2 != 0) {
            std::cerr << "tsort: odd number of tokens: input contains an incomplete pair\n";
            return 1;
        }

        GraphNodeManager nodes;
        std::vector<std::unordered_set<int>> adj;
        std::vector<int> inDegree;

        for (size_t i = 0; i < allTokens.size(); i += 2) {
            int u = nodes.getOrCreateId(allTokens[i]);
            int v = nodes.getOrCreateId(allTokens[i + 1]);

            while (adj.size() <= static_cast<size_t>((std::max)(u, v))) {
                adj.emplace_back();
                inDegree.push_back(0);
            }

            if (u != v) {
                if (adj[u].find(v) == adj[u].end()) {
                    adj[u].insert(v);
                    inDegree[v]++;
                }
            }
        }

        std::queue<int> q;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (i < inDegree.size() && inDegree[i] == 0) {
                q.push(static_cast<int>(i));
            }
        }

        std::vector<std::string> ordered;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            ordered.push_back(nodes.getName(u));

            if (static_cast<size_t>(u) < adj.size()) {
                for (int v : adj[u]) {
                    inDegree[v]--;
                    if (inDegree[v] == 0) {
                        q.push(v);
                    }
                }
            }
        }

        if (ordered.size() < nodes.size()) {
            std::cerr << "tsort: input contains a loop\n";
        }

        return TsortReporter::dispatch(ordered, options.outputFormat, options.pipeCommand);
    }
