#include "document.h"

Document::Document(int id, const std::string& path) : FilePath(path), WindowId(id) {
    if (path.empty()) {
        Title = "NO NAME " + std::to_string(id) + ".TXT";
        Lines.push_back("");
    } else {
        Title = fs::path(path).filename().string();
        LoadFile(path);
    }
}

void Document::LoadFile(const std::string& path) {
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
bool Document::SaveFile() {
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

void Document::RecordEdit() {
    // Snapshot copies only shared line pointers (O(line count), not O(bytes)).
    UndoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
    if (UndoStack.size() > 200) {
        UndoStack.pop_front();
    }
    RedoStack.clear();
}

bool Document::Undo() {
    if (UndoStack.empty()) return false;
    RedoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
    if (RedoStack.size() > 200) {
        RedoStack.pop_front();
    }
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

bool Document::Redo() {
    if (RedoStack.empty()) return false;
    UndoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
    if (UndoStack.size() > 200) {
        UndoStack.pop_front();
    }
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

void Document::InvalidateLineStatesFrom(size_t index) const {
    if (index < m_stateDirtyFrom) m_stateDirtyFrom = index;
}

HighlightState Document::GetLineState(size_t line) const {
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
