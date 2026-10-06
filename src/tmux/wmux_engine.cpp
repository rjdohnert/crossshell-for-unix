#include "theme_catalog.hpp"
#include "theme_lookup.hpp"
#include "ui_theme.hpp"
#include "wmux_engine.hpp"

WmuxEngine::WmuxEngine() {
        hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }

void WmuxEngine::MarkDirty() {
        isDirty = true;
        cvDirty.notify_one();
    }

const UiTheme& WmuxEngine::Theme() const {
        const auto& themes = AvailableThemes();
        return themes[activeThemeIdx % themes.size()];
    }

bool WmuxEngine::SetTheme(const std::string& name) {
        int idx = ThemeIndexByName(name);
        if (idx < 0) return false;
        activeThemeIdx = (size_t)idx;
        MarkDirty();
        return true;
    }

void WmuxEngine::CycleTheme() {
        const auto& themes = AvailableThemes();
        activeThemeIdx = (activeThemeIdx + 1) % themes.size();
        MarkDirty();
    }
