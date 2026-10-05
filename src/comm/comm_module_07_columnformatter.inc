    class ColumnFormatter {
    private:
        const Options& opts_;
        std::string col1_prefix_;
        std::string col2_prefix_;
        std::string col3_prefix_;
        uint64_t count_col1_ = 0;
        uint64_t count_col2_ = 0;
        uint64_t count_col3_ = 0;

    public:
        explicit ColumnFormatter(const Options& opts) : opts_(opts) {
            // Calculate column prefixes based on active suppression flags
            // Col 1 never has leading tabs
            col1_prefix_ = "";

            // Col 2 prefix has delimiter only if Col 1 is visible
            col2_prefix_ = opts_.show_col1 ? opts_.col_delimiter : "";

            // Col 3 prefix has delimiters for each visible preceding column
            col3_prefix_ = (opts_.show_col1 ? opts_.col_delimiter : "") +
                           (opts_.show_col2 ? opts_.col_delimiter : "");
        }

        void print_col1(const std::string& line) {
            count_col1_++;
            if (opts_.show_col1) {
                std::cout << col1_prefix_ << line << opts_.delimiter;
            }
        }

        void print_col2(const std::string& line) {
            count_col2_++;
            if (opts_.show_col2) {
                std::cout << col2_prefix_ << line << opts_.delimiter;
            }
        }

        void print_col3(const std::string& line) {
            count_col3_++;
            if (opts_.show_col3) {
                std::cout << col3_prefix_ << line << opts_.delimiter;
            }
        }

        void print_totals() const {
            if (opts_.show_total) {
                std::cout << count_col1_ << opts_.col_delimiter
                          << count_col2_ << opts_.col_delimiter
                          << count_col3_ << opts_.col_delimiter
                          << "total" << opts_.delimiter;
            }
        }
    };

    // =========================================================================
    // Core Engine (Three-Way Merge Algorithm)
    // =========================================================================
