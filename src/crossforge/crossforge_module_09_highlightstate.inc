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
inline void ScanHighlightLineState(const std::string& line, HighlightState& state) {
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

// Persistent line buffer: each line is an immutable shared_ptr<const string>.
// Snapshots (undo/redo) copy only the pointer vector, so unchanged lines are
// shared across history entries instead of being deep-copied on every keystroke.
// Edits always replace a line slot with a fresh string (copy-on-write), which
// keeps history snapshots valid.
