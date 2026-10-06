#include "layout_node.hpp"
#include "pane.hpp"
#include "wmux_engine.hpp"

void WmuxEngine::HandleMouseEvent(const MOUSE_EVENT_RECORD& mer) {
        std::lock_guard<std::recursive_mutex> lock(stateMtx);
        int mx = mer.dwMousePosition.X;
        int my = mer.dwMousePosition.Y;
        auto& win = windows[activeWindowIdx];

        if (my == termH - 1 && (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
            int currentX = 4;
            for (size_t i = 0; i < windows.size(); ++i) {
                int len = (int)windows[i]->name.size() + 5;
                if (mx >= currentX && mx < currentX + len) {
                    activeWindowIdx = i;
                    RecalculateLayout();
                    break;
                }
                currentX += len;
            }
            return;
        }

        // Handle Border Dragging
        if (draggingBorderNode) {
            if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                if (draggingBorderNode->split == SplitType::Vertical) {
                    float r = (float)(mx - draggingBorderNode->x) / (float)(draggingBorderNode->w > 0 ? draggingBorderNode->w : 1);
                    draggingBorderNode->ratio = std::clamp(r, 0.1f, 0.9f);
                } else if (draggingBorderNode->split == SplitType::Horizontal) {
                    float r = (float)(my - draggingBorderNode->y) / (float)(draggingBorderNode->h > 0 ? draggingBorderNode->h : 1);
                    draggingBorderNode->ratio = std::clamp(r, 0.1f, 0.9f);
                }
                RecalculateLayout();
            } else {
                draggingBorderNode = nullptr;
            }
            return;
        }

        if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
            auto hitBorder = win->rootLayout->HitTestBorder(mx, my);
            if (hitBorder) {
                draggingBorderNode = hitBorder;
                return;
            }
        }

        // Find Target Pane
        std::shared_ptr<Pane> targetPane = nullptr;
        for (auto& p : win->GetPanes()) {
            if (p->bounds.Contains(mx, my)) {
                targetPane = p;
                break;
            }
        }

        if (!targetPane) return;

        if (win->activePane != targetPane && (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
            win->activePane = targetPane;
            MarkDirty();
        }

        // In-App Mouse Passthrough into Child Process (Lazygit, Micro, Far, Vim)
        if (targetPane->screen.mouseTracking && targetPane->screen.mouseSgrMode) {
            int relX = mx - targetPane->bounds.x + 1; // 1-based coordinates
            int relY = my - targetPane->bounds.y + 1;

            if (relX >= 1 && relX <= targetPane->screen.width && relY >= 1 && relY <= targetPane->screen.height) {
                int btn = 0;
                bool isRelease = false;

                if (mer.dwEventFlags == MOUSE_WHEELED) {
                    short wheel = (short)HIWORD(mer.dwButtonState);
                    btn = (wheel > 0) ? 64 : 65;
                } else if (mer.dwEventFlags == MOUSE_MOVED) {
                    if (!targetPane->screen.mouseAllMotion && !targetPane->screen.mouseButtonMotion) {
                        return;
                    }
                    if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                        btn = 32 + 0;
                    } else if (mer.dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
                        btn = 32 + 2;
                    } else if (mer.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) {
                        btn = 32 + 1; // Middle button
                    } else {
                        if (targetPane->screen.mouseAllMotion) {
                            btn = 35; // Move with no buttons pressed
                        } else {
                            return;
                        }
                    }
                } else {
                    if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) btn = 0;
                    else if (mer.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) btn = 1;
                    else if (mer.dwButtonState & RIGHTMOST_BUTTON_PRESSED) btn = 2;
                    else isRelease = true;
                }

                std::string sgrSeq = "\x1b[<" + std::to_string(btn) + ";" + std::to_string(relX) + ";" + std::to_string(relY) + (isRelease ? "m" : "M");
                targetPane->SendInput(sgrSeq);
                return;
            }
        }

        // Default Scrollback for Multiplexer
        if (mer.dwEventFlags == MOUSE_WHEELED) {
            short wheel = (short)HIWORD(mer.dwButtonState);
            std::lock_guard<std::mutex> lock(targetPane->screen.mtx);
            if (wheel > 0) targetPane->screen.scrollOffset = std::min((int)targetPane->screen.scrollback.size(), targetPane->screen.scrollOffset + 3);
            else targetPane->screen.scrollOffset = std::max(0, targetPane->screen.scrollOffset - 3);
            MarkDirty();
        }
    }
