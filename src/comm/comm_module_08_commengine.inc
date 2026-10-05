    class CommEngine {
    private:
        Options opts_;
        std::unique_ptr<ILineStream> stream1_;
        std::unique_ptr<ILineStream> stream2_;
        ColumnFormatter formatter_;
        OrderValidator validator1_;
        OrderValidator validator2_;

    public:
        explicit CommEngine(Options opts)
            : opts_(opts),
              stream1_(std::make_unique<LineStream>(opts_.file1, opts_.delimiter)),
              stream2_(std::make_unique<LineStream>(opts_.file2, opts_.delimiter)),
              formatter_(opts_),
              validator1_(opts_.file1, opts_.check_order, opts_.ignore_case),
              validator2_(opts_.file2, opts_.check_order, opts_.ignore_case) {}

        ExitCode execute() {
            if (opts_.file1 == "-" && opts_.file2 == "-") {
                std::cerr << "comm: cannot compare standard input to itself\n";
                return ExitCode::Failure;
            }

            if (!stream1_->open()) {
                std::cerr << "comm: " << opts_.file1 << ": No such file or directory\n";
                return ExitCode::Failure;
            }
            if (!stream2_->open()) {
                std::cerr << "comm: " << opts_.file2 << ": No such file or directory\n";
                return ExitCode::Failure;
            }

            std::string line1, line2;
            bool has1 = stream1_->get_next_line(line1);
            bool has2 = stream2_->get_next_line(line2);

            bool order_ok = true;

            while (has1 && has2) {
                if (!validator1_.validate(line1, stream1_->get_line_number())) order_ok = false;
                if (!validator2_.validate(line2, stream2_->get_line_number())) order_ok = false;

                int comp = StringComparator::compare(line1, line2, opts_.ignore_case);

                if (comp < 0) {
                    formatter_.print_col1(line1);
                    has1 = stream1_->get_next_line(line1);
                } else if (comp > 0) {
                    formatter_.print_col2(line2);
                    has2 = stream2_->get_next_line(line2);
                } else {
                    formatter_.print_col3(line1);
                    has1 = stream1_->get_next_line(line1);
                    has2 = stream2_->get_next_line(line2);
                }
            }

            // Flush remaining lines from stream1
            while (has1) {
                if (!validator1_.validate(line1, stream1_->get_line_number())) order_ok = false;
                formatter_.print_col1(line1);
                has1 = stream1_->get_next_line(line1);
            }

            // Flush remaining lines from stream2
            while (has2) {
                if (!validator2_.validate(line2, stream2_->get_line_number())) order_ok = false;
                formatter_.print_col2(line2);
                has2 = stream2_->get_next_line(line2);
            }

            formatter_.print_totals();

            return (order_ok || !opts_.check_order) ? ExitCode::Success : ExitCode::Failure;
        }
    };

    // =========================================================================
    // Command Line Argument Parser
    // =========================================================================
