#pragma once

#include "pane.hpp"
#include "rect.hpp"
#include "tmux.hpp"

enum class SplitType { None, Horizontal, Vertical };

struct LayoutNode : public std::enable_shared_from_this<LayoutNode> {
    SplitType split = SplitType::None;
    float ratio = 0.5f;
    std::shared_ptr<LayoutNode> first;
    std::shared_ptr<LayoutNode> second;
    std::shared_ptr<Pane> pane;
    std::weak_ptr<LayoutNode> parent;
    Rect borderRect{};
    int x = 0, y = 0, w = 0, h = 0;

    void CalculateBounds(int nx, int ny, int nw, int nh);

    void CollectPanes(std::vector<std::shared_ptr<Pane>>& list);

    std::shared_ptr<LayoutNode> FindNodeForPane(std::shared_ptr<Pane> p);

    std::shared_ptr<LayoutNode> HitTestBorder(int px, int py);
};
