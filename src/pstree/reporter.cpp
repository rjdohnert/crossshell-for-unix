#include "reporter.hpp"

TreeRenderer::TreeRenderer(const Config& config, ProcessMap& map) 
    : m_config(config), m_map(map) 
{
    InitializeSymbols();
}

void TreeRenderer::InitializeSymbols() noexcept {
    if (m_config.style == TreeStyle::ASCII) {
        m_sym.branch   = L"|-- ";
        m_sym.last     = L"`-- ";
        m_sym.vertical = L"|   ";
        m_sym.space    = L"    ";
    } else {
        // Unicode Standard Default
        m_sym.branch   = L"├── ";
        m_sym.last     = L"└── ";
        m_sym.vertical = L"│   ";
        m_sym.space    = L"    ";
    }
}

void TreeRenderer::BuildTreeStructure() {
    for (auto& [pid, node] : m_map) {
        if (m_config.showFullPaths) {
            node.fullPath = ProcessTreeEngine::QueryProcessImagePath(pid);
        }

        // Mark orphan or top-level process nodes
        if (node.ppid == 0 || node.ppid == node.pid || m_map.find(node.ppid) == m_map.end()) {
            node.isRootCandidate = true;
        } else {
            m_map[node.ppid].children.push_back(pid);
        }
    }

    // Sort children for every process node
    for (auto& [pid, node] : m_map) {
        SortPids(node.children);
    }
}

void TreeRenderer::SortPids(std::vector<DWORD>& pids) {
    if (m_config.numericSort) {
        std::sort(pids.begin(), pids.end());
    } else {
        std::sort(pids.begin(), pids.end(), [this](DWORD a, DWORD b) {
            const auto& nameA = m_map[a].name;
            const auto& nameB = m_map[b].name;
            int cmp = _wcsicmp(nameA.c_str(), nameB.c_str());
            if (cmp != 0) return cmp < 0;
            return a < b; // Secondary tie-breaker by PID
        });
    }
}

void TreeRenderer::PrintNodeLabel(const ProcessNode& node) {
    std::wstring displayName = (m_config.showFullPaths && !node.fullPath.empty()) 
                                ? node.fullPath 
                                : node.name;
    std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towupper(ch));
    });

    if (m_config.colorOutput) {
        const bool isSystemProcess = (node.pid == 0 || node.pid == 4);
        const wchar_t* color = isSystemProcess ? Color::RED : Color::CYAN;
        const wchar_t* weight = isSystemProcess ? Color::BOLD : L"";

        std::wcout << weight << color << displayName;
        if (m_config.showPids) {
            std::wcout << L"(" << node.pid << L")";
        }
        std::wcout << Color::RESET;
    } else {
        std::wcout << displayName;
        if (m_config.showPids) {
            std::wcout << L"(" << node.pid << L")";
        }
    }
    std::wcout << L"\n";
}

void TreeRenderer::RenderNode(DWORD pid, const std::wstring& prefix, bool isLast, std::unordered_set<DWORD>& visited) {
    if (visited.count(pid)) {
        // Guard against stack overflow due to recycled PIDs
        std::wcout << prefix << (isLast ? m_sym.last : m_sym.branch) << L"[CYCLE DETECTED: PID " << pid << L"]\n";
        return;
    }
    visited.insert(pid);

    auto it = m_map.find(pid);
    if (it == m_map.end()) return;

    const auto& node = it->second;

    if (!prefix.empty()) {
        std::wcout << prefix << (isLast ? m_sym.last : m_sym.branch);
    }

    PrintNodeLabel(node);

    std::wstring childPrefix = prefix + (isLast ? m_sym.space : m_sym.vertical);
    const auto& children = node.children;

    for (size_t i = 0; i < children.size(); ++i) {
        bool childIsLast = (i == children.size() - 1);
        RenderNode(children[i], childPrefix, childIsLast, visited);
    }
}

void TreeRenderer::RenderWithAncestors(DWORD targetPid) {
    std::vector<DWORD> ancestors;
    DWORD current = targetPid;

    std::unordered_set<DWORD> loopCheck;
    while (current != 0 && m_map.find(current) != m_map.end()) {
        if (loopCheck.count(current)) break;
        loopCheck.insert(current);

        ancestors.push_back(current);
        DWORD parent = m_map[current].ppid;
        if (parent == current) break;
        current = parent;
    }

    std::reverse(ancestors.begin(), ancestors.end());

    std::wstring currentPrefix = L"";
    std::unordered_set<DWORD> visited;

    for (size_t i = 0; i < ancestors.size(); ++i) {
        DWORD pid = ancestors[i];
        const auto& node = m_map[pid];

        if (i > 0) {
            std::wcout << currentPrefix << m_sym.last;
        }

        PrintNodeLabel(node);

        if (pid == targetPid) {
            // Render full child subtree for target PID
            std::wstring childPrefix = currentPrefix + (i > 0 ? m_sym.space : L"");
            const auto& children = node.children;
            for (size_t c = 0; c < children.size(); ++c) {
                bool childIsLast = (c == children.size() - 1);
                RenderNode(children[c], childPrefix, childIsLast, visited);
            }
        } else {
            currentPrefix += m_sym.space;
        }
    }
}

void TreeRenderer::Render() {
    BuildTreeStructure();

    if (m_config.targetPid != 0) {
        if (m_map.find(m_config.targetPid) == m_map.end()) {
            std::wcerr << L"pstree: Process ID " << m_config.targetPid << L" not found in process table.\n";
            exit(3);
        }

        if (m_config.showParents) {
            RenderWithAncestors(m_config.targetPid);
            return;
        } else {
            std::unordered_set<DWORD> visited;
            RenderNode(m_config.targetPid, L"", true, visited);
            return;
        }
    }

    // Render full system process forest
    std::unordered_set<DWORD> visited;
    std::vector<DWORD> rootPids;

    for (const auto& [pid, node] : m_map) {
        if (node.isRootCandidate) {
            rootPids.push_back(pid);
        }
    }

    SortPids(rootPids);

    for (size_t i = 0; i < rootPids.size(); ++i) {
        bool isLast = (i == rootPids.size() - 1);
        RenderNode(rootPids[i], L"", isLast, visited);
    }
}
