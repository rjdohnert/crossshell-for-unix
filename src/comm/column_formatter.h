#ifndef COMM_COLUMN_FORMATTER_H
#define COMM_COLUMN_FORMATTER_H

#include "comm.h"
#include "options.h"

namespace CommUtil {

// Column Output Formatter
class ColumnFormatter {
public:
    explicit ColumnFormatter(const Options& opts);

    void print_col1(const std::string& line);
    void print_col2(const std::string& line);
    void print_col3(const std::string& line);
    void print_totals() const;

private:
    const Options& opts_;
    std::string col1_prefix_;
    std::string col2_prefix_;
    std::string col3_prefix_;
    uint64_t count_col1_ = 0;
    uint64_t count_col2_ = 0;
    uint64_t count_col3_ = 0;
};

} // namespace CommUtil

#endif // COMM_COLUMN_FORMATTER_H
