#include "comm_engine.h"
#include "string_comparator.h"

namespace CommUtil {

CommEngine::CommEngine(Options opts)
    : opts_(opts),
      stream1_(std::make_unique<LineStream>(opts_.file1, opts_.delimiter)),
      stream2_(std::make_unique<LineStream>(opts_.file2, opts_.delimiter)),
      formatter_(opts_),
      validator1_(opts_.file1, opts_.check_order, opts_.ignore_case),
      validator2_(opts_.file2, opts_.check_order, opts_.ignore_case) {}

ExitCode CommEngine::execute() {
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

    while (has1 && has2) {
        // Validators warn on disorder but do not stop the comparison (GNU behavior).
        validator1_.validate(line1, stream1_->get_line_number());
        validator2_.validate(line2, stream2_->get_line_number());

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
        validator1_.validate(line1, stream1_->get_line_number());
        formatter_.print_col1(line1);
        has1 = stream1_->get_next_line(line1);
    }

    // Flush remaining lines from stream2
    while (has2) {
        validator2_.validate(line2, stream2_->get_line_number());
        formatter_.print_col2(line2);
        has2 = stream2_->get_next_line(line2);
    }

    formatter_.print_totals();

    // GNU comm exits 0 even when input is unsorted (it only warns). Real I/O
    // errors above already returned Failure, so reaching here means success.
    return ExitCode::Success;
}

} // namespace CommUtil
