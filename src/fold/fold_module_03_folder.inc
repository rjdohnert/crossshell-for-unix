    // =========================================================================
    // Text Folder Engine (Stream Processor)
    // =========================================================================
    class TextFolder {
    public:
        explicit TextFolder(const Options& opts) : m_opts(opts) {}

        void process_stream(std::istream& in, std::ostream& out) {
            std::string line_buffer;
            size_t current_col = 0;
            std::optional<size_t> last_space_index;

            char ch;
            while (in.get(ch)) {
                // Line breaks reset state and flush current buffer
                if (ch == '\n') {
                    out << line_buffer << '\n';
                    line_buffer.clear();
                    current_col = 0;
                    last_space_index.reset();
                    continue;
                }

                size_t char_width = calculate_char_width(ch, current_col);

                // Check if adding this character exceeds target width
                if (current_col + char_width > m_opts.width && !line_buffer.empty()) {
                    if (m_opts.break_at_spaces && last_space_index.has_value()) {
                        // Word boundary wrap: flush up to the last seen blank character
                        size_t split_pos = last_space_index.value();
                        out << line_buffer.substr(0, split_pos + 1) << '\n';

                        // Retain leftovers after the space for the next line
                        std::string remainder = line_buffer.substr(split_pos + 1);
                        line_buffer = remainder;
                        current_col = recalculate_width(remainder);
                        last_space_index.reset();
                        
                        // Recheck any spaces inside the carried over remainder
                        for (size_t i = 0; i < line_buffer.size(); ++i) {
                            if (is_blank(line_buffer[i])) {
                                last_space_index = i;
                            }
                        }

                        char_width = calculate_char_width(ch, current_col);
                    } else {
                        // Hard wrap
                        out << line_buffer << '\n';
                        line_buffer.clear();
                        current_col = 0;
                        last_space_index.reset();
                        char_width = calculate_char_width(ch, current_col);
                    }
                }

                // Update column track
                if (m_opts.count_bytes) {
                    current_col += 1;
                } else {
                    if (ch == '\b') {
                        current_col = (current_col > 0) ? current_col - 1 : 0;
                    } else if (ch == '\r') {
                        current_col = 0;
                    } else if (ch == '\t') {
                        current_col = next_tab_stop(current_col);
                    } else {
                        current_col += 1;
                    }
                }

                line_buffer.push_back(ch);

                if (is_blank(ch)) {
                    last_space_index = line_buffer.size() - 1;
                }
            }

            // Flush remaining buffer at EOF
            if (!line_buffer.empty()) {
                out << line_buffer;
            }
        }

    private:
        const Options& m_opts;

        static bool is_blank(char ch) noexcept {
            return ch == ' ' || ch == '\t';
        }

        static size_t next_tab_stop(size_t col) noexcept {
            return (col / 8 + 1) * 8;
        }

        size_t calculate_char_width(char ch, size_t col) const noexcept {
            if (m_opts.count_bytes) return 1;
            if (ch == '\t') return next_tab_stop(col) - col;
            if (ch == '\b' || ch == '\r') return 0;
            return 1;
        }

        size_t recalculate_width(std::string_view sv) const noexcept {
            if (m_opts.count_bytes) return sv.size();
            size_t col = 0;
            for (char ch : sv) {
                if (ch == '\t') col = next_tab_stop(col);
                else if (ch == '\b') col = (col > 0) ? col - 1 : 0;
                else if (ch == '\r') col = 0;
                else col += 1;
            }
            return col;
        }
    };
