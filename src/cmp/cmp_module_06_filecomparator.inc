    class FileComparator {
    private:
        Options opts_;
        std::unique_ptr<IByteStream> stream1_;
        std::unique_ptr<IByteStream> stream2_;

    public:
        explicit FileComparator(Options opts)
            : opts_(std::move(opts)),
              stream1_(std::make_unique<ChunkedStream>(opts_.file1)),
              stream2_(std::make_unique<ChunkedStream>(opts_.file2)) {}

        ExitCode run() {
            if (!stream1_->open()) {
                std::cerr << "cmp: " << opts_.file1 << ": No such file or directory or access denied\n";
                return ExitCode::Error;
            }
            if (!stream2_->open()) {
                std::cerr << "cmp: " << opts_.file2 << ": No such file or directory or access denied\n";
                return ExitCode::Error;
            }

            // Apply offsets
            stream1_->skip(opts_.skip1);
            stream2_->skip(opts_.skip2);

            uint64_t byte_count = 0;
            uint64_t line_count = 1;
            bool files_differ = false;

            while (byte_count < opts_.limit_bytes) {
                uint8_t b1 = 0, b2 = 0;
                bool read1 = stream1_->read_byte(b1);
                bool read2 = stream2_->read_byte(b2);

                if (!read1 || !read2) {
                    if (read1 != read2) {
                        files_differ = true;
                        if (!opts_.silent) {
                            const std::string& short_file = (!read1) ? opts_.file1 : opts_.file2;
                            std::cerr << "cmp: EOF on " << short_file;
                            if (byte_count > 0) {
                                std::cerr << " after byte " << byte_count << ", line " << line_count;
                            } else {
                                std::cerr << " which is empty";
                            }
                            std::cerr << "\n";
                        }
                    }
                    break;
                }

                byte_count++;

                if (b1 != b2) {
                    files_differ = true;
                    if (opts_.silent) {
                        return ExitCode::Differ;
                    }

                    if (opts_.verbose_all) {
                        // Print decimal byte offset and both bytes in octal
                        std::cout << std::dec << std::setw(6) << byte_count << " "
                                  << std::oct << std::setw(3) << static_cast<int>(b1) << " "
                                  << std::oct << std::setw(3) << static_cast<int>(b2) << "\n";
                    } else {
                        // Standard default single difference format
                        std::cout << opts_.file1 << " " << opts_.file2 << " differ: byte " 
                                  << std::dec << byte_count << ", line " << line_count;
                        
                        if (opts_.print_bytes) {
                            std::cout << " is " << std::oct << static_cast<int>(b1) << " " << Formatter::byte_representation(b1)
                                      << " " << std::oct << static_cast<int>(b2) << " " << Formatter::byte_representation(b2);
                        }
                        std::cout << "\n";
                        return ExitCode::Differ;
                    }
                }

                if (b1 == '\n') {
                    line_count++;
                }
            }

            return files_differ ? ExitCode::Differ : ExitCode::Identical;
        }
    };

    // =========================================================================
    // Command Line Argument Parser
    // =========================================================================
