#include "cat_options.hpp"

void CatOptions::normalizeDependencies() {
    if (numberNonBlank) {
        numberLines = true;
    }
    if (showEnds || showTabs) {
        showNonPrinting = true;
    }
}

bool CatOptions::requiresFormatting() const {
    return outputJson || numberLines || showEnds || squeezeBlanks || showTabs || showNonPrinting;
}
