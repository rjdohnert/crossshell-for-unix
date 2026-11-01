#ifndef COMM_ORDER_VALIDATOR_H
#define COMM_ORDER_VALIDATOR_H

#include "comm.h"

namespace CommUtil {

// Sort Order Monitor
class OrderValidator {
public:
    OrderValidator(std::string stream_name, bool check_order, bool ignore_case);

    // Check that `current_line` is not before the previously seen line.
    // Matches GNU comm: on the first disorder it prints a warning to stderr
    // but the comparison CONTINUES (returns true); only the warning is recorded.
    bool validate(const std::string& current_line, uint64_t line_number);

    bool has_error() const { return order_error_found_; }

private:
    std::string previous_line_;
    bool has_previous_ = false;
    bool order_error_found_ = false;
    bool ignore_case_;
    bool check_order_;
    std::string stream_name_;
};

} // namespace CommUtil

#endif // COMM_ORDER_VALIDATOR_H
