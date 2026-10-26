#include "wslmgr_common.hpp"

void ApplyGlyphs(GlyphStyle style) {
    switch (style) {
    case GlyphStyle::Ascii:
        Glyph::check = "OK";
        Glyph::cross = "X";
        Glyph::dot = "*";
        Glyph::arrow = ">";
        Glyph::active = ">";
        Glyph::menu = "[]";
        Glyph::warn = "!";
        Glyph::theme = "T";
        break;
    case GlyphStyle::NerdFont:
        Glyph::check = "";
        Glyph::cross = "";
        Glyph::dot = "";
        Glyph::arrow = "";
        Glyph::active = "";
        Glyph::menu = "";
        Glyph::warn = "";
        Glyph::theme = "";
        break;
    case GlyphStyle::Unicode:
    default:
        Glyph::check = "✓";
        Glyph::cross = "✗";
        Glyph::dot = "•";
        Glyph::arrow = "→";
        Glyph::active = "▶";
        Glyph::menu = "▣";
        Glyph::warn = "⚠";
        Glyph::theme = "◐";
        break;
    }
}
