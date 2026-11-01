#ifndef CROSSFORGE_THEME_MANAGER_H
#define CROSSFORGE_THEME_MANAGER_H

#include "crossforge.h"
#include "supported_language.h"
#include "theme_palette.h"

class ThemeManager {
public:
    static ThemePalette GetTheme(ThemeScheme scheme);
};

#endif // CROSSFORGE_THEME_MANAGER_H
