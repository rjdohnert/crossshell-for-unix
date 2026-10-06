#include "counters.hpp"
#include "text_metrics_scanner.hpp"

void TextMetricsScanner::ScanStream(std::istream& in, Counters& c) {
        char buffer[16384];
        bool in_word = false;
        long long current_line_len = 0;

        int utf8_expected_continuations = 0;
        bool utf8_pending_starter = false;

        auto advance_display_width = [&current_line_len](unsigned char ch) {
            if (ch == '\t') {
                long long tab_advance = 8 - (current_line_len % 8);
                current_line_len += tab_advance;
                return;
            }
            if (ch < 0x20 || ch == 0x7F || ch == '\r' || ch == '\n') {
                return;
            }
            current_line_len++;
        };

        auto count_completed_codepoint = [&c, &current_line_len]() {
            c.chars++;
            current_line_len++;
        };

        while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
            std::streamsize bytes_read = in.gcount();
            c.bytes += bytes_read;

            for (std::streamsize i = 0; i < bytes_read; ++i) {
                unsigned char ch = static_cast<unsigned char>(buffer[i]);

                bool is_space = (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f');
                if (is_space) {
                    in_word = false;
                } else if (!in_word) {
                    in_word = true;
                    c.words++;
                }

                if (ch == '\n') {
                    if (utf8_pending_starter) {
                        utf8_expected_continuations = 0;
                        utf8_pending_starter = false;
                        count_completed_codepoint();
                    }
                    c.lines++;
                    if (current_line_len > c.max_line_length) {
                        c.max_line_length = current_line_len;
                    }
                    current_line_len = 0;
                }

                if (utf8_expected_continuations > 0) {
                    if ((ch & 0xC0) == 0x80) {
                        utf8_expected_continuations--;
                        if (utf8_expected_continuations == 0) {
                            utf8_pending_starter = false;
                            count_completed_codepoint();
                        }
                        continue;
                    } else {
                        utf8_expected_continuations = 0;
                        if (utf8_pending_starter) {
                            utf8_pending_starter = false;
                            count_completed_codepoint();
                        }
                    }
                }

                if (ch < 0x80) {
                    c.chars++;
                    if (ch != '\n') {
                        advance_display_width(ch);
                    }
                } else if (ch >= 0xC2 && ch <= 0xDF) {
                    utf8_expected_continuations = 1;
                    utf8_pending_starter = true;
                } else if (ch >= 0xE0 && ch <= 0xEF) {
                    utf8_expected_continuations = 2;
                    utf8_pending_starter = true;
                } else if (ch >= 0xF0 && ch <= 0xF4) {
                    utf8_expected_continuations = 3;
                    utf8_pending_starter = true;
                } else {
                    c.chars++;
                    if (ch != '\n') {
                        advance_display_width(ch);
                    }
                }
            }
        }

        if (utf8_pending_starter) {
            count_completed_codepoint();
        }

        if (current_line_len > c.max_line_length) {
            c.max_line_length = current_line_len;
        }
    }
