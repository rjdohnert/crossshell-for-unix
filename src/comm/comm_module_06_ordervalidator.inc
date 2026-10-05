    class OrderValidator {
    private:
        std::string previous_line_;
        bool has_previous_ = false;
        bool order_error_found_ = false;
        bool ignore_case_;
        bool check_order_;
        std::string stream_name_;

    public:
        OrderValidator(std::string stream_name, bool check_order, bool ignore_case)
            : stream_name_(std::move(stream_name)),
              ignore_case_(ignore_case),
              check_order_(check_order) {}

        bool validate(const std::string& current_line, uint64_t line_number) {
            if (!check_order_) {
                return true;
            }

            if (has_previous_) {
                if (StringComparator::compare(previous_line_, current_line, ignore_case_) > 0) {
                    if (!order_error_found_) {
                        std::cerr << "comm: file '" << stream_name_
                                  << "' is not in sorted order on line " << line_number << "\n";
                        order_error_found_ = true;
                    }
                    return false;
                }
            }

            previous_line_ = current_line;
            has_previous_ = true;
            return true;
        }

        bool has_error() const { return order_error_found_; }
    };

    // =========================================================================
    // Column Output Formatter
    // =========================================================================
