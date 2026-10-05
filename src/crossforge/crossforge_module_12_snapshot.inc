    struct Snapshot {
        LineBuffer Lines;
        int CursorX = 0;
        int CursorY = 0;
        int ScrollY = 0;
        int ScrollX = 0;
        bool IsModified = false;
    };

    std::string FilePath;
    std::string Title;
    LineBuffer Lines;
    int CursorX = 0;
    int CursorY = 0;
    int ScrollY = 0;
    int ScrollX = 0;
    bool IsModified = false;
    SupportedLanguage Lang = SupportedLanguage::Text;
    int WindowId = 1;
    std::deque<Snapshot> UndoStack;
    std::deque<Snapshot> RedoStack;

    // Incremental syntax-highlight line-state cache. m_lineStates[i] is the
    // HighlightState at the START of line i. m_stateDirtyFrom is the first line
    // whose entry state may have changed; lines before it are known-good, so a
    // query for the state at line L rescans only from m_stateDirtyFrom, not from 0.
    mutable std::vector<HighlightState> m_lineStates;
    mutable size_t m_stateDirtyFrom = 0;

    // Mark that lines from `index` onward may have changed, invalidating their
    // cached entry states (cheap: just lowers the dirty watermark).
    void InvalidateLineStatesFrom(size_t index) const {
        if (index < m_stateDirtyFrom) m_stateDirtyFrom = index;
    }

    // Return the HighlightState at the start of line `line`, rescanning only from
    // the first dirty line. Amortized O(1) per frame when edits are localized.
    HighlightState GetLineState(size_t line) const {
        size_t count = Lines.size();
        if (m_lineStates.size() < count + 1) m_lineStates.resize(count + 1);
        size_t from = (std::min)(m_stateDirtyFrom, line);
        HighlightState s = m_lineStates[from];
        for (size_t i = from; i < line; ++i) {
            ScanHighlightLineState(Lines[i], s);
            m_lineStates[i + 1] = s;
        }
        // Everything up to `line` is now known-good.
        if (m_stateDirtyFrom < line) m_stateDirtyFrom = line;
        return s;
    }

    Document(int id, const std::string& path = "") : WindowId(id), FilePath(path) {
        if (path.empty()) {
            Title = "NO NAME " + std::to_string(id) + ".TXT";
            Lines.push_back("");
        } else {
            Title = fs::path(path).filename().string();
            LoadFile(path);
        }
    }

    void LoadFile(const std::string& path) {
        FilePath = path;
        Lines.clear();
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            std::string line;
            bool firstLine = true;
            while (std::getline(file, line)) {
                // Strip a UTF-8 BOM from the first line so it isn't saved back as content.
                if (firstLine && line.size() >= 3 &&
                    static_cast<unsigned char>(line[0]) == 0xEF &&
                    static_cast<unsigned char>(line[1]) == 0xBB &&
                    static_cast<unsigned char>(line[2]) == 0xBF) {
                    line = line.substr(3);
                }
                // Drop a trailing CR so CRLF files don't carry \r into the buffer.
                if (!line.empty() && line.back() == '\r') line.pop_back();
                Lines.push_back(line);
                firstLine = false;
            }
            file.close();
        }
        if (Lines.empty()) Lines.push_back("");
        // Fresh content: drop any cached highlight line-states.
        m_lineStates.clear();
        m_stateDirtyFrom = 0;
    }

    // Save the buffer. Returns true only if the file was fully written; leaves
    // IsModified set on failure so the user isn't told a bad save succeeded.
    bool SaveFile() {
        if (FilePath.empty()) return false;
        std::ofstream file(FilePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;
        for (size_t i = 0; i < Lines.size(); ++i) {
            file << Lines[i];
            if (i + 1 < Lines.size()) file << "\n";
        }
        file.flush();
        if (file.fail()) {
            file.close();
            return false; // keep IsModified set; caller can warn the user
        }
        file.close();
        IsModified = false;
        return true;
    }

    void RecordEdit() {
        // Snapshot copies only shared line pointers (O(line count), not O(bytes)).
        UndoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
        if (UndoStack.size() > 200) {
            UndoStack.pop_front();
        }
        RedoStack.clear();
    }

    bool Undo() {
        if (UndoStack.empty()) return false;
        RedoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
        Snapshot state = std::move(UndoStack.back());
        UndoStack.pop_back();
        Lines = std::move(state.Lines);
        CursorX = state.CursorX;
        CursorY = state.CursorY;
        ScrollY = state.ScrollY;
        ScrollX = state.ScrollX;
        IsModified = state.IsModified; // restore, don't force-modified
        InvalidateLineStatesFrom(0);
        return true;
    }

    bool Redo() {
        if (RedoStack.empty()) return false;
        UndoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
        Snapshot state = std::move(RedoStack.back());
        RedoStack.pop_back();
        Lines = std::move(state.Lines);
        CursorX = state.CursorX;
        CursorY = state.CursorY;
        ScrollY = state.ScrollY;
        ScrollX = state.ScrollX;
        IsModified = state.IsModified; // restore, don't force-modified
        InvalidateLineStatesFrom(0);
        return true;
    }
};

// ============================================================================
// TUI WINDOW & RENDERING ENGINE
// ============================================================================

