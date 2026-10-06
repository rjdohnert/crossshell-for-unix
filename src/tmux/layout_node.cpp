#include "layout_node.hpp"
#include "pane.hpp"

void LayoutNode::CalculateBounds(int nx, int ny, int nw, int nh) {
        x = nx; y = ny; w = nw; h = nh;
        if (split == SplitType::None) {
            if (pane) {
                pane->bounds = { x, y, w, h };
                pane->Resize(w, h);
            }
            return;
        }

        if (split == SplitType::Vertical) { // Left | Right
            int firstW = (int)std::round((w - 1) * ratio);
            firstW = std::max(2, std::min(w - 3, firstW));
            int secondW = w - firstW - 1;
            borderRect = { x + firstW, y, 1, h };
            if (first) first->CalculateBounds(x, y, firstW, h);
            if (second) second->CalculateBounds(x + firstW + 1, y, secondW, h);
        } else if (split == SplitType::Horizontal) { // Top / Bottom
            int firstH = (int)std::round((h - 1) * ratio);
            firstH = std::max(2, std::min(h - 3, firstH));
            int secondH = h - firstH - 1;
            borderRect = { x, y + firstH, w, 1 };
            if (first) first->CalculateBounds(x, y, w, firstH);
            if (second) second->CalculateBounds(x, y + firstH + 1, w, secondH);
        }
    }

void LayoutNode::CollectPanes(std::vector<std::shared_ptr<Pane>>& list) {
        if (split == SplitType::None && pane) list.push_back(pane);
        else {
            if (first) first->CollectPanes(list);
            if (second) second->CollectPanes(list);
        }
    }

std::shared_ptr<LayoutNode> LayoutNode::FindNodeForPane(std::shared_ptr<Pane> p) {
        if (split == SplitType::None && pane == p) return shared_from_this();
        if (first) { auto n = first->FindNodeForPane(p); if (n) return n; }
        if (second) { auto n = second->FindNodeForPane(p); if (n) return n; }
        return nullptr;
    }

std::shared_ptr<LayoutNode> LayoutNode::HitTestBorder(int px, int py) {
        if (split != SplitType::None) {
            if (borderRect.Contains(px, py)) return shared_from_this();
            if (first) { auto n = first->HitTestBorder(px, py); if (n) return n; }
            if (second) { auto n = second->HitTestBorder(px, py); if (n) return n; }
        }
        return nullptr;
    }
