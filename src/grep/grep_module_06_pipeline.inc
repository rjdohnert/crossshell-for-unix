// ============================================================================
// 6. PIPELINE & STREAM PROCESSOR
// ============================================================================
struct ContextLine {
    long long line_num;
    std::string line;
};

class StreamProcessor {
public:
    StreamProcessor(const GrepOptions& options, GrepEngine& grepEngine)
        : opts(options), engine(grepEngine) {}

    void CollectFilesRecursive(const fs::path& dir, std::vector<std::string>& files) const {
        std::error_code ec;
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
    }

    long long ProcessStream(std::istream& in, const std::string& filename, bool show_filename) {
        std::string raw_line;
        long long line_num = 0;
        long long match_count = 0;
        unsigned long long byte_offset = 0;
        int after_context_left = 0;
        std::deque<ContextLine> before_buffer;

        while (std::getline(in, raw_line)) {
            const unsigned long long line_offset = byte_offset;
            byte_offset += raw_line.size() + 1;
            if (!opts.text_mode && raw_line.find('\0') != std::string::npos) {
                if (opts.binary_files_without_match) return 0;
                if (!opts.binary_mode) {
                    if (!opts.no_messages && !opts.quiet) std::cerr << "grep: " << filename << ": binary file matches\n";
                    return 1;
                }
            }
            if (!raw_line.empty() && raw_line.back() == '\r') raw_line.pop_back();
            line_num++;

            MatchResult res = engine.evaluate_line(raw_line);

            if (res.matched) {
                match_count++;

                if (opts.files_with_matches) {
                    if (!opts.quiet) std::cout << filename << (opts.null_output ? '\0' : '\n');
                    return match_count;
                }
                if (opts.quiet) return match_count;
                if (opts.count_only) continue;

                while (!before_buffer.empty()) {
                    auto& b = before_buffer.front();
                    if (show_filename) std::cout << COLOR_FILE << filename << COLOR_SEP << "-" << COLOR_RESET;
                    if (opts.line_number) std::cout << COLOR_LINE << b.line_num << COLOR_SEP << "-" << COLOR_RESET;
                    std::cout << b.line << (opts.null_output ? '\0' : '\n');
                    before_buffer.pop_front();
                }

                if (show_filename) std::cout << COLOR_FILE << filename << COLOR_SEP << ":" << COLOR_RESET;
                if (opts.byte_offset) std::cout << line_offset << ":";
                if (opts.line_number) std::cout << COLOR_LINE << line_num << COLOR_SEP << ":" << COLOR_RESET;

                bool offsets_match_display = opts.key_path.empty() && opts.extract_path.empty();
                if (opts.only_matching && offsets_match_display) {
                    for (auto& span : res.match_spans) {
                        std::cout << (opts.color ? COLOR_MATCH : "")
                                  << res.text_to_display.substr(span.first, span.second)
                                  << (opts.color ? COLOR_RESET : "") << (opts.null_output ? '\0' : '\n');
                    }
                } else {
                    if (opts.color && !res.match_spans.empty() && offsets_match_display) {
                        std::sort(res.match_spans.begin(), res.match_spans.end());
                        std::vector<std::pair<size_t, size_t>> merged;
                        for (const auto& span : res.match_spans) {
                            if (span.first >= res.text_to_display.size()) continue;
                            size_t end = (std::min)(res.text_to_display.size(), span.first + span.second);
                            if (merged.empty() || span.first > merged.back().first + merged.back().second) merged.push_back({span.first, end - span.first});
                            else {
                                size_t merged_end = (std::max)(merged.back().first + merged.back().second, end);
                                merged.back().second = merged_end - merged.back().first;
                            }
                        }
                        size_t last = 0;
                        for (const auto& span : merged) {
                            if (span.first > last) std::cout << res.text_to_display.substr(last, span.first - last);
                            std::cout << COLOR_MATCH << res.text_to_display.substr(span.first, span.second) << COLOR_RESET;
                            last = span.first + span.second;
                        }
                        if (last < res.text_to_display.size()) std::cout << res.text_to_display.substr(last);
                        std::cout << (opts.null_output ? '\0' : '\n');
                    } else {
                        std::cout << res.text_to_display << (opts.null_output ? '\0' : '\n');
                    }
                }

                after_context_left = opts.after_context;
                if (opts.max_count > 0 && match_count >= opts.max_count) break;

            } else {
                if (opts.files_without_match || opts.count_only || opts.files_with_matches) continue;

                if (after_context_left > 0) {
                    if (show_filename) std::cout << COLOR_FILE << filename << COLOR_SEP << "-" << COLOR_RESET;
                    if (opts.line_number) std::cout << COLOR_LINE << line_num << COLOR_SEP << "-" << COLOR_RESET;
                    std::cout << raw_line << (opts.null_output ? '\0' : '\n');
                    after_context_left--;
                } else if (opts.before_context > 0) {
                    before_buffer.push_back({line_num, raw_line});
                    if (static_cast<int>(before_buffer.size()) > opts.before_context) {
                        before_buffer.pop_front();
                    }
                }
            }
        }

        if (opts.files_without_match && match_count == 0) {
            if (!opts.quiet) std::cout << filename << (opts.null_output ? '\0' : '\n');
            return 1;
        } else if (opts.count_only) {
            if (show_filename) std::cout << COLOR_FILE << filename << COLOR_SEP << ":" << COLOR_RESET;
            if (!opts.quiet) std::cout << match_count << (opts.null_output ? '\0' : '\n');
        }
        return match_count;
    }

private:
    const GrepOptions& opts;
    GrepEngine& engine;
};

