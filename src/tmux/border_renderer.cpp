#include "cell.hpp"
#include "color.hpp"
#include "layout_node.hpp"
#include "pane.hpp"
#include "rect.hpp"
#include "ui_theme.hpp"
#include "wmux_engine.hpp"

bool WmuxEngine::IsBorderChar(wchar_t ch) {
        return ch == L'-' || ch == L'|' || ch == L'+';
    }

void WmuxEngine::SmoothBorders(std::vector<std::vector<Cell>>& frame) {
        int sbY = termH - 1; // Status bar row
        for (int y = 0; y < sbY; ++y) {
            for (int x = 0; x < termW; ++x) {
                wchar_t ch = frame[y][x].ch;
                if (!IsBorderChar(ch)) {
                    continue;
                }

                bool u = (y > 0) && IsBorderChar(frame[y - 1][x].ch);
                bool d = (y < sbY - 1) && IsBorderChar(frame[y + 1][x].ch);
                bool l = (x > 0) && IsBorderChar(frame[y][x - 1].ch);
                bool r = (x < termW - 1) && IsBorderChar(frame[y][x + 1].ch);

                wchar_t newCh = ch;
                if (u && d && l && r) newCh = L'+';
                else if (d && l && r) newCh = L'+';
                else if (u && l && r) newCh = L'+';
                else if (u && d && r) newCh = L'+';
                else if (u && d && l) newCh = L'+';
                else if (d && r) newCh = L'+';
                else if (d && l) newCh = L'+';
                else if (u && r) newCh = L'+';
                else if (u && l) newCh = L'+';
                else if (u || d) newCh = L'|';
                else if (l || r) newCh = L'-';

                frame[y][x].ch = newCh;
            }
        }
    }

void WmuxEngine::DrawLayoutDividers(std::shared_ptr<LayoutNode> node, std::vector<std::vector<Cell>>& frame, std::shared_ptr<Pane> activePane) {
        if (!node || node->split == SplitType::None) return;

        bool highlight = false;
        if (activePane) {
            Rect ab = activePane->bounds;
            Rect br = node->borderRect;
            if (node->split == SplitType::Vertical) {
                if (ab.x + ab.width == br.x || ab.x == br.x + 1) {
                    highlight = true;
                }
            } else if (node->split == SplitType::Horizontal) {
                if (ab.y + ab.height == br.y || ab.y == br.y + 1) {
                    highlight = true;
                }
            }
        }

        const UiTheme& theme = Theme();
        Color borderCol = highlight ? theme.activeBorder : theme.border;
        Rect br = node->borderRect;

        if (node->split == SplitType::Vertical) {
            for (int y = br.y; y < br.y + br.height && y < termH; ++y) {
                if (br.x >= 0 && br.x < termW) {
                    frame[y][br.x] = Cell{ L'|', borderCol, Color::DefaultBg() };
                }
            }
        } else if (node->split == SplitType::Horizontal) {
            for (int x = br.x; x < br.x + br.width && x < termW; ++x) {
                if (br.y >= 0 && br.y < termH) {
                    frame[br.y][x] = Cell{ L'-', borderCol, Color::DefaultBg() };
                }
            }
        }

        DrawLayoutDividers(node->first, frame, activePane);
        DrawLayoutDividers(node->second, frame, activePane);
    }
