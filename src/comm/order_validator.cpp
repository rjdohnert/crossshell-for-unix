#include "order_validator.h"
#include "string_comparator.h"

namespace CommUtil {

OrderValidator::OrderValidator(std::string stream_name, bool check_order, bool ignore_case)
    : stream_name_(std::move(stream_name)),
      ignore_case_(ignore_case),
      check_order_(check_order) {}

bool OrderValidator::validate(const std::string& current_line, uint64_t line_number) {
    if (!check_order_) {
        return true;
    }

    if (has_previous_) {
        if (StringComparator::compare(previous_line_, current_line, ignore_case_) > 0) {
            // GNU comm behavior: warn once on the first disorder, but keep
            // comparing and let the run succeed. The warning is recorded in
            // order_error_found_ for callers that want to inspect it.
            if (!order_error_found_) {
                std::cerr << "comm: file '" << stream_name_
                          << "' is not in sorted order on line " << line_number << "\n";
                order_error_found_ = true;
            }
            // Update the reference point and continue (do not fail the run).
            previous_line_ = current_line;
            return true;
        }
    }

    previous_line_ = current_line;
    has_previous_ = true;
    return true;
}

} // namespace CommUtil
