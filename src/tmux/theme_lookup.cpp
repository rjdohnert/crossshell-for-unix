#include "string_utils.hpp"
#include "theme_catalog.hpp"
#include "theme_lookup.hpp"

int ThemeIndexByName(const std::string& name) {
    std::string needle = LowerCopy(TrimCopy(name));
    const auto& themes = AvailableThemes();
    for (size_t i = 0; i < themes.size(); ++i) {
        if (needle == themes[i].name) return (int)i;
    }
    return -1;
}
