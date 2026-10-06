#include "umask_parser.hpp"

unsigned int UmaskParser::ParseSymbolicMask(unsigned int current_mask, const std::string& expr) {
        unsigned int current_perm = (~current_mask) & MASK_ALL_BITS;
        std::stringstream ss(expr);
        std::string clause;

        while (std::getline(ss, clause, ',')) {
            if (clause.empty()) continue;

            size_t pos = 0;
            unsigned int who_mask = 0;

            while (pos < clause.size() && (clause[pos] == 'u' || clause[pos] == 'g' || clause[pos] == 'o' || clause[pos] == 'a')) {
                if (clause[pos] == 'u') who_mask |= 0700;
                else if (clause[pos] == 'g') who_mask |= 0070;
                else if (clause[pos] == 'o') who_mask |= 0007;
                else if (clause[pos] == 'a') who_mask |= 0777;
                pos++;
            }
            if (who_mask == 0) who_mask = 0777;

            if (pos >= clause.size()) {
                throw std::invalid_argument("Invalid symbolic clause: '" + clause + "'");
            }
            char op = clause[pos++];
            if (op != '=' && op != '+' && op != '-') {
                throw std::invalid_argument("Invalid operator '" + std::string(1, op) + "' in clause: '" + clause + "'");
            }

            unsigned int perm_bits = 0;
            while (pos < clause.size()) {
                char c = clause[pos++];
                if (c == 'r') perm_bits |= 0444;
                else if (c == 'w') perm_bits |= 0222;
                else if (c == 'x') perm_bits |= 0111;
                else if (c == 'u') perm_bits |= ((current_perm >> 6) & 7) * 0111;
                else if (c == 'g') perm_bits |= ((current_perm >> 3) & 7) * 0111;
                else if (c == 'o') perm_bits |= (current_perm & 7) * 0111;
                else {
                    throw std::invalid_argument("Invalid permission character '" + std::string(1, c) + "'");
                }
            }

            unsigned int affected_bits = perm_bits & who_mask;
            if (op == '=') {
                current_perm = (current_perm & ~who_mask) | affected_bits;
            } else if (op == '+') {
                current_perm |= affected_bits;
            } else if (op == '-') {
                current_perm &= ~affected_bits;
            }
        }

        return (~current_perm) & MASK_ALL_BITS;
    }

unsigned int UmaskParser::ParseMaskInput(unsigned int current_mask, const std::string& input) {
        bool is_octal = !input.empty() && std::all_of(input.begin(), input.end(), [](char c) {
            return c >= '0' && c <= '7';
        });

        if (is_octal) {
            unsigned int val = std::stoul(input, nullptr, 8);
            return val & MASK_ALL_BITS;
        }

        return ParseSymbolicMask(current_mask, input);
    }
