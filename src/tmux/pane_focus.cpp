#include "pane.hpp"
#include "rect.hpp"
#include "wmux_engine.hpp"

void WmuxEngine::MoveFocus(int dirKey) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        auto& win = windows[activeWindowIdx];
        if (!win || !win->activePane || win->isZoomed) return;

        auto activePane = win->activePane;
        Rect ab = activePane->bounds;

        std::shared_ptr<Pane> bestPane = nullptr;
        int bestDist = 999999;

        for (auto& p : win->GetPanes()) {
            if (p == activePane) continue;
            Rect pb = p->bounds;

            bool isCandidate = false;
            int dist = 0;

            if (dirKey == VK_LEFT) {
                if (pb.x + pb.width <= ab.x + 1) {
                    isCandidate = true;
                    dist = (ab.x - (pb.x + pb.width)) * 10 + std::abs((pb.y + pb.height/2) - (ab.y + ab.height/2));
                }
            } else if (dirKey == VK_RIGHT) {
                if (pb.x >= ab.x + ab.width - 1) {
                    isCandidate = true;
                    dist = (pb.x - (ab.x + ab.width)) * 10 + std::abs((pb.y + pb.height/2) - (ab.y + ab.height/2));
                }
            } else if (dirKey == VK_UP) {
                if (pb.y + pb.height <= ab.y + 1) {
                    isCandidate = true;
                    dist = (ab.y - (pb.y + pb.height)) * 10 + std::abs((pb.x + pb.width/2) - (ab.x + ab.width/2));
                }
            } else if (dirKey == VK_DOWN) {
                if (pb.y >= ab.y + ab.height - 1) {
                    isCandidate = true;
                    dist = (pb.y - (ab.y + ab.height)) * 10 + std::abs((pb.x + pb.width/2) - (ab.x + ab.width/2));
                }
            }

            if (isCandidate && dist < bestDist) {
                bestDist = dist;
                bestPane = p;
            }
        }

        if (bestPane) {
            win->activePane = bestPane;
            MarkDirty();
        }
    }
