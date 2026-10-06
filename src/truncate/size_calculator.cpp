#include "size_calculator.hpp"

SizeCalculator::SizeCalculator(SizeOp operation, uint64_t val) : op(operation), operand(val) {}

uint64_t SizeCalculator::compute_new_size(uint64_t current_size) const {
        switch (op) {
            case SizeOp::Exact:
                return operand;
            case SizeOp::Add:
                return current_size + operand;
            case SizeOp::Subtract:
                return (current_size > operand) ? (current_size - operand) : 0ULL;
            case SizeOp::RoundDown:
                if (operand == 0) return current_size;
                return current_size - (current_size % operand);
            case SizeOp::RoundUp: {
                if (operand == 0) return current_size;
                uint64_t remainder = current_size % operand;
                return (remainder == 0) ? current_size : (current_size + (operand - remainder));
            }
        }
        return current_size;
    }

bool SizeCalculator::parse(const std::string& spec, SizeCalculator& out_calc, std::string& err) {
        if (spec.empty()) {
            err = "empty size specification";
            return false;
        }

        SizeOp op = SizeOp::Exact;
        size_t idx = 0;

        char first = spec[0];
        if (first == '+') { op = SizeOp::Add; idx++; }
        else if (first == '-') { op = SizeOp::Subtract; idx++; }
        else if (first == '%') { op = SizeOp::RoundDown; idx++; }
        else if (first == '/') { op = SizeOp::RoundUp; idx++; }

        if (idx >= spec.length()) {
            err = "missing numeric size after operator '" + std::string(1, first) + "'";
            return false;
        }

        // Parse numerical part
        size_t num_start = idx;
        while (idx < spec.length() && std::isdigit(static_cast<unsigned char>(spec[idx]))) {
            idx++;
        }

        if (num_start == idx) {
            err = "invalid number format in size '" + std::string(spec) + "'";
            return false;
        }

        uint64_t base_val = 0;
        try {
            base_val = std::stoull(std::string(spec.substr(num_start, idx - num_start)));
        } catch (...) {
            err = "size value out of range";
            return false;
        }

        // Parse BSD suffixes (case-insensitive)
        uint64_t multiplier = 1;
        if (idx < spec.length()) {
            char suffix = static_cast<char>(std::tolower(static_cast<unsigned char>(spec[idx])));
            switch (suffix) {
                case 'b': multiplier = 512ULL; break;                      // BSD 512-byte blocks
                case 'k': multiplier = 1024ULL; break;                     // Kilobytes
                case 'm': multiplier = 1024ULL * 1024ULL; break;           // Megabytes
                case 'g': multiplier = 1024ULL * 1024ULL * 1024ULL; break;  // Gigabytes
                case 't': multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL; break; // Terabytes
                case 'p': multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL; break; // Petabytes
                case 'e': multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL; break; // Exabytes
                default:
                    err = "invalid size suffix '" + std::string(1, spec[idx]) + "'";
                    return false;
            }
            idx++;
        }

        if (idx < spec.length()) {
            err = "trailing characters in size specification '" + std::string(spec) + "'";
            return false;
        }

        if ((op == SizeOp::RoundDown || op == SizeOp::RoundUp) && (base_val * multiplier == 0)) {
            err = "rounding scale cannot be zero";
            return false;
        }

        out_calc = SizeCalculator(op, base_val * multiplier);
        return true;
    }
