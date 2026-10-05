    class ChunkedStream : public IByteStream {
    private:
        static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64 KB buffer
        std::string name_;
        std::istream* stream_ = nullptr;
        std::unique_ptr<std::ifstream> file_stream_;
        std::vector<uint8_t> buffer_;
        size_t buf_pos_ = 0;
        size_t buf_len_ = 0;
        bool is_stdin_ = false;
        bool eof_reached_ = false;
        bool error_ = false;

    public:
        explicit ChunkedStream(std::string name)
            : name_(std::move(name)), buffer_(BUFFER_SIZE) {
            if (name_ == "-") {
                is_stdin_ = true;
            }
        }

        bool open() override {
            if (is_stdin_) {
#ifdef _WIN32
                // Force stdin to binary mode on Windows to avoid CRLF translations & EOF truncations
                if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
                    error_ = true;
                    return false;
                }
#endif
                stream_ = &std::cin;
                return true;
            } else {
                file_stream_ = std::make_unique<std::ifstream>(name_, std::ios::binary);
                if (!file_stream_->is_open()) {
                    error_ = true;
                    return false;
                }
                stream_ = file_stream_.get();
                return true;
            }
        }

        bool read_byte(uint8_t& byte) override {
            if (buf_pos_ >= buf_len_) {
                if (eof_reached_ || !refill_buffer()) {
                    return false;
                }
            }
            byte = buffer_[buf_pos_++];
            return true;
        }

        void skip(uint64_t count) override {
            uint8_t dummy;
            while (count > 0 && read_byte(dummy)) {
                --count;
            }
        }

        bool is_eof() const override {
            return eof_reached_ && (buf_pos_ >= buf_len_);
        }

        const std::string& get_name() const override {
            return name_;
        }

        bool has_error() const override {
            return error_;
        }

    private:
        bool refill_buffer() {
            if (!stream_ || !(*stream_)) {
                eof_reached_ = true;
                return false;
            }

            stream_->read(reinterpret_cast<char*>(buffer_.data()), BUFFER_SIZE);
            std::streamsize bytes_read = stream_->gcount();

            if (bytes_read <= 0) {
                eof_reached_ = true;
                return false;
            }

            buf_pos_ = 0;
            buf_len_ = static_cast<size_t>(bytes_read);
            return true;
        }
    };

    // =========================================================================
    // Format Utility
    // =========================================================================
