#ifndef CROSSFORGE_DOCUMENT_H
#define CROSSFORGE_DOCUMENT_H

#include "crossforge.h"
#include "supported_language.h"
#include "highlight_state.h"

// Persistent line buffer: each line is an immutable shared_ptr<const string>.
// Snapshots (undo/redo) copy only the pointer vector, so unchanged lines are
// shared across history entries instead of being deep-copied on every keystroke.
// Edits always replace a line slot with a fresh string (copy-on-write), which
// keeps history snapshots valid.
class Document {
public:
    using LinePtr = std::shared_ptr<const std::string>;

    // Line storage wrapper that mimics the subset of std::vector<std::string>
    // used by the editor while storing lines as immutable shared pointers.
    class LineBuffer {
    public:
        size_t size() const { return m_lines.size(); }
        bool empty() const { return m_lines.empty(); }

        // Reading a line yields a const std::string& (zero-copy). Returns a static
        // empty string for an out-of-range index instead of UB.
        const std::string& operator[](size_t i) const {
            static const std::string kEmpty;
            return (i < m_lines.size() && m_lines[i]) ? *m_lines[i] : kEmpty;
        }

        // Assigning replaces the slot with a fresh immutable line.
        void set(size_t i, std::string s) {
            if (i < m_lines.size()) {
                m_lines[i] = std::make_shared<const std::string>(std::move(s));
            }
        }

        void push_back(const std::string& s) { m_lines.push_back(std::make_shared<const std::string>(s)); }

        void insert(size_t index, const std::string& s) {
            if (index > m_lines.size()) index = m_lines.size();
            m_lines.insert(m_lines.begin() + index, std::make_shared<const std::string>(s));
        }

        void erase(size_t index) {
            if (index < m_lines.size()) m_lines.erase(m_lines.begin() + index);
        }

        void clear() { m_lines.clear(); }

        void assignSingle(const std::string& s) {
            m_lines.clear();
            push_back(s);
        }

        void resetSingle(const std::string& s) { assignSingle(s); }

        // Replace every line with a fresh string (used by replace-all).
        void replaceEach(const std::function<std::string(const std::string&)>& fn) {
            for (size_t i = 0; i < m_lines.size(); ++i) {
                if (m_lines[i]) {
                    set(i, fn(*m_lines[i]));
                }
            }
        }

        // Approximate live byte size of all line content.
        size_t byteSize() const {
            size_t total = 0;
            for (const auto& p : m_lines) total += p ? p->size() : 0;
            return total;
        }

    private:
        std::vector<LinePtr> m_lines;
    };

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

    Document(int id, const std::string& path = "");

    void LoadFile(const std::string& path);
    bool SaveFile();
    void RecordEdit();
    bool Undo();
    bool Redo();

    // Mark that lines from `index` onward may have changed, invalidating their
    // cached entry states (cheap: just lowers the dirty watermark).
    void InvalidateLineStatesFrom(size_t index) const;

    // Return the HighlightState at the start of line `line`, rescanning only from
    // the first dirty line. Amortized O(1) per frame when edits are localized.
    HighlightState GetLineState(size_t line) const;

private:
    // Incremental syntax-highlight line-state cache. m_lineStates[i] is the
    // HighlightState at the START of line i. m_stateDirtyFrom is the first line
    // whose entry state may have changed; lines before it are known-good, so a
    // query for the state at line L rescans only from m_stateDirtyFrom, not from 0.
    mutable std::vector<HighlightState> m_lineStates;
    mutable size_t m_stateDirtyFrom = 0;
};

#endif // CROSSFORGE_DOCUMENT_H
