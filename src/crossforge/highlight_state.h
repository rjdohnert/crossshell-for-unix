#ifndef CROSSFORGE_HIGHLIGHT_STATE_H
#define CROSSFORGE_HIGHLIGHT_STATE_H

#include "crossforge.h"

// Cross-line lexical state for the syntax highlighter. Carried from line to
// line so block comments and multi-line strings stay highlighted.
struct HighlightState {
    bool inBlockComment = false; // inside /* ... */
    char stringQuote = '\0';     // inside a string: '"' or '\'' ('\0' = none)
    bool operator==(const HighlightState& o) const {
        return inBlockComment == o.inBlockComment && stringQuote == o.stringQuote;
    }
    bool operator!=(const HighlightState& o) const { return !(*this == o); }
};

// Advance `state` by scanning a whole line without rendering. Handles // and #
// line comments, /* */ block comments, and strings (with backslash escapes, so
// an escaped quote doesn't end a string). A string open at EOL remains open.
void ScanHighlightLineState(const std::string& line, HighlightState& state);

#endif // CROSSFORGE_HIGHLIGHT_STATE_H
