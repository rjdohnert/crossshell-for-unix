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
        void set(size_t i, std::string s) { m_lines[i] = std::make_shared<const std::string>(std::move(s)); }

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
                set(i, fn(*m_lines[i]));
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

