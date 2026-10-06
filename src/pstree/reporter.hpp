#pragma once

#include "pstree.hpp"
#include "engine.hpp"

class TreeRenderer {
public:
    TreeRenderer(const Config& config, ProcessMap& map);
    void Render();

private:
    const Config& m_config;
    ProcessMap& m_map;
    TreeSymbols m_sym;

    void InitializeSymbols() noexcept;
    void BuildTreeStructure();
    void SortPids(std::vector<DWORD>& pids);
    void PrintNodeLabel(const ProcessNode& node);
    void RenderNode(DWORD pid, const std::wstring& prefix, bool isLast, std::unordered_set<DWORD>& visited);
    void RenderWithAncestors(DWORD targetPid);
};
