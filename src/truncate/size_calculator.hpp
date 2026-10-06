#pragma once

#include "truncate.hpp"

enum class SizeOp {
    Exact,      // -s <size>  : Set exact size
    Add,        // -s +<size> : Increase size by <size>
    Subtract,   // -s -<size> : Decrease size by at most <size> (min 0)
    RoundDown,  // -s %<size> : Round size down to multiple of <size>
    RoundUp     // -s /<size> : Round size up to multiple of <size>
};

class SizeCalculator {
private:
    SizeOp op = SizeOp::Exact;
    uint64_t operand = 0;

public:
    SizeCalculator() = default;
    SizeCalculator(SizeOp operation, uint64_t val);

    uint64_t compute_new_size(uint64_t current_size) const;

    static bool parse(const std::string& spec, SizeCalculator& out_calc, std::string& err);
};
