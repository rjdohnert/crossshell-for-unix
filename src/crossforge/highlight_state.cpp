#include "highlight_state.h"

// Advance `state` by scanning a whole line without rendering. Handles // and #
// line comments, /* */ block comments, and strings (with backslash escapes, so
// an escaped quote doesn't end a string). A string open at EOL remains open.
void ScanHighlightLineState(const std::string& line, HighlightState& state) {
    size_t idx = 0;
    const size_t n = line.length();
    while (idx < n) {
        if (state.inBlockComment) {
            size_t close = line.find("*/", idx);
            if (close == std::string::npos) { return; } // still in comment at EOL
            state.inBlockComment = false;
            idx = close + 2;
            continue;
        }
        if (state.stringQuote != '\0') {
            char q = state.stringQuote;
            char c = line[idx];
            if (c == '\\' && idx + 1 < n) { idx += 2; continue; } // skip escape
            if (c == q) { state.stringQuote = '\0'; }
            ++idx;
            continue;
        }
        // Not in a continuing construct.
        if (line[idx] == '/' && idx + 1 < n && line[idx + 1] == '/') {
            return; // line comment: rest of line, state unchanged
        }
        if (line[idx] == '#') {
            return;
        }
        if (line[idx] == '/' && idx + 1 < n && line[idx + 1] == '*') {
            state.inBlockComment = true;
            idx += 2;
            continue;
        }
        if (line[idx] == '"' || line[idx] == '\'') {
            state.stringQuote = line[idx];
            ++idx;
            continue;
        }
        ++idx;
    }
}
