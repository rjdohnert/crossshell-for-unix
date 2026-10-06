#include "layout_node.hpp"
#include "pane.hpp"
#include "string_encoding.hpp"
#include "window.hpp"
#include "wmux_engine.hpp"

void WmuxEngine::CreateWindowTab(const std::string& name, const std::wstring& shellCmd) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        auto win = std::make_shared<Window>(nextWindowId++, name);
        auto p = std::make_shared<Pane>(nextPaneId++, WStringToString(shellCmd), [this]() { MarkDirty(); });
        win->rootLayout->pane = p;
        win->activePane = p;
        windows.push_back(win);
        activeWindowIdx = windows.size() - 1;

        RecalculateLayout();
        p->Start(shellCmd, p->bounds.width, p->bounds.height);
        MarkDirty();
    }

void WmuxEngine::RecalculateLayout() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        if (windows.empty()) return;
        auto& win = windows[activeWindowIdx];
        int availH = termH - 1;

        if (win->isZoomed && win->activePane) {
            win->activePane->bounds = { 0, 0, termW, availH };
            win->activePane->Resize(termW, availH);
        } else {
            win->rootLayout->CalculateBounds(0, 0, termW, availH);
        }
        MarkDirty();
    }

void WmuxEngine::SplitActivePane(SplitType type) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        auto& win = windows[activeWindowIdx];
        if (win->isZoomed) win->isZoomed = false;

        auto node = win->rootLayout->FindNodeForPane(win->activePane);
        if (!node) return;

        auto existingPane = node->pane;
        auto newPane = std::make_shared<Pane>(nextPaneId++, WStringToString(defaultShell), [this]() { MarkDirty(); });

        node->split = type;
        node->ratio = 0.5f;
        node->pane = nullptr;

        node->first = std::make_shared<LayoutNode>();
        node->first->pane = existingPane;
        node->first->parent = node;

        node->second = std::make_shared<LayoutNode>();
        node->second->pane = newPane;
        node->second->parent = node;

        win->activePane = newPane;
        RecalculateLayout();
        newPane->Start(defaultShell, newPane->bounds.width, newPane->bounds.height);
        MarkDirty();
    }

void WmuxEngine::KillPane(std::shared_ptr<Pane> p) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        for (size_t wIdx = 0; wIdx < windows.size(); ++wIdx) {
            auto win = windows[wIdx];
            auto panes = win->GetPanes();
            auto it = std::find(panes.begin(), panes.end(), p);
            if (it != panes.end()) {
                if (panes.size() <= 1) {
                    windows.erase(windows.begin() + wIdx);
                    if (windows.empty()) { isRunning = false; return; }
                    activeWindowIdx = std::min(activeWindowIdx, windows.size() - 1);
                    RecalculateLayout();
                    return;
                }

                auto node = win->rootLayout->FindNodeForPane(p);
                if (!node) return;

                auto parent = node->parent.lock();
                if (!parent) return;

                auto sibling = (parent->first == node) ? parent->second : parent->first;
                parent->split = sibling->split;
                parent->ratio = sibling->ratio;
                parent->pane = sibling->pane;
                parent->first = sibling->first;
                parent->second = sibling->second;
                if (parent->first) parent->first->parent = parent;
                if (parent->second) parent->second->parent = parent;

                auto remaining = win->GetPanes();
                if (win->activePane == p) {
                    win->activePane = remaining.front();
                }
                RecalculateLayout();
                MarkDirty();
                return;
            }
        }
    }

void WmuxEngine::KillActivePane() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        if (windows.empty()) return;
        auto& win = windows[activeWindowIdx];
        if (win->activePane) {
            KillPane(win->activePane);
        }
    }

void WmuxEngine::RequestKillActivePaneConfirm() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        if (windows.empty()) return;
        auto& win = windows[activeWindowIdx];
        if (!win->activePane) return;
        pendingKillPaneTarget = win->activePane;
        pendingKillPaneConfirm = true;
        MarkDirty();
    }

void WmuxEngine::ResolveKillPaneConfirm(bool confirmed) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        pendingKillPaneConfirm = false;
        auto target = pendingKillPaneTarget;
        pendingKillPaneTarget.reset();
        if (confirmed && target) KillPane(target);
        MarkDirty();
    }

void WmuxEngine::ReapDeadPanes() {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        std::vector<std::shared_ptr<Pane>> deadPanes;
        for (auto& win : windows) {
            for (auto& p : win->GetPanes()) {
                if (!p->alive) {
                    deadPanes.push_back(p);
                }
            }
        }
        for (auto& p : deadPanes) {
            KillPane(p);
        }
    }
