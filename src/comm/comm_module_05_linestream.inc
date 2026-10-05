    class LineStream : public ILineStream {
    private:
        std::string name_;
        char line_delim_;
        std::istream* stream_ = nullptr;
        std::unique_ptr<std::ifstream> file_stream_;
        uint64_t line_number_ = 0;
        bool is_stdin_ = false;
        bool eof_ = false;

    public:
        LineStream(std::string name, char line_delim)
            : name_(std::move(name)), line_delim_(line_delim) {
            if (name_ == "-") {
                is_stdin_ = true;
            }
        }

        bool open() override {
            if (is_stdin_) {
#ifdef _WIN32
                // Enable binary stream on Windows for accurate byte-level piping
                if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
                    return false;
                }
#endif
                stream_ = &std::cin;
                return true;
            } else {
                file_stream_ = std::make_unique<std::ifstream>(name_, std::ios::binary);
                if (!file_stream_->is_open()) {
                    return false;
                }
                stream_ = file_stream_.get();
                return true;
            }
        }

        bool get_next_line(std::string& out_line) override {
            if (!stream_ || eof_) {
                return false;
            }

            if (std::getline(*stream_, out_line, line_delim_)) {
                // When line delimiter is newline '\n', trim trailing '\r' for CRLF compatibility
                if (line_delim_ == '\n' && !out_line.empty() && out_line.back() == '\r') {
                    out_line.pop_back();
                }
                line_number_++;
                return true;
            }

            eof_ = true;
            return false;
        }

        const std::string& get_name() const override { return name_; }
        uint64_t get_line_number() const override { return line_number_; }
        bool is_eof() const override { return eof_; }
    };

    // =========================================================================
    // Sort Order Monitor
    // =========================================================================
