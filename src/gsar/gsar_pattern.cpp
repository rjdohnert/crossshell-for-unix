#include "gsar_pattern.hpp"

#include <cctype>

namespace {
int HexValue(unsigned char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

bool AppendNumericEscape(std::string_view input, size_t& index, uint8_t& value,
                         unsigned base, size_t maximumDigits, std::string& error) {
    size_t digits = 0;
    unsigned parsed = 0;
    while (index + 1 < input.size() && digits < maximumDigits) {
        const unsigned char character = static_cast<unsigned char>(input[index + 1]);
        unsigned digit = 0;
        if (base == 16) {
            const int hex = HexValue(character);
            if (hex < 0) break;
            digit = static_cast<unsigned>(hex);
        } else if (character >= '0' && character <= '9') {
            digit = character - '0';
            if (digit >= base) break;
        } else {
            break;
        }

        if (parsed > (255u - digit) / base) {
            error = "numeric byte escape is outside the range 0-255";
            return false;
        }
        parsed = parsed * base + digit;
        ++index;
        ++digits;
    }

    if (digits == 0) {
        error = base == 16 ? "hexadecimal escape requires digits"
                           : "numeric escape requires digits";
        return false;
    }
    if (base == 16 && digits != 2) {
        error = "backslash hexadecimal escapes require exactly two digits";
        return false;
    }

    value = static_cast<uint8_t>(parsed);
    return true;
}
}

bool PatternParser::parse(std::string_view input, std::vector<uint8_t>& bytes,
                          std::string& error) {
    bytes.clear();
    error.clear();
    bytes.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        const char introducer = input[i];
        if ((introducer != '\\' && introducer != ':') || i + 1 >= input.size()) {
            bytes.push_back(static_cast<uint8_t>(introducer));
            continue;
        }

        const char escape = input[i + 1];
        if (introducer == ':' && escape == ':') {
            bytes.push_back(static_cast<uint8_t>(':'));
            ++i;
            continue;
        }

        if (introducer == ':' && (escape == 'x' || escape == 'X')) {
            size_t digits = 0;
            size_t cursor = i + 2;
            while (cursor < input.size() &&
                   HexValue(static_cast<unsigned char>(input[cursor])) >= 0) {
                ++digits;
                ++cursor;
            }
            if (digits == 0 || digits % 2 != 0) {
                error = "colon hexadecimal escapes require one or more complete byte pairs";
                return false;
            }
            for (size_t digit = i + 2; digit < cursor; digit += 2) {
                const int high = HexValue(static_cast<unsigned char>(input[digit]));
                const int low = HexValue(static_cast<unsigned char>(input[digit + 1]));
                bytes.push_back(static_cast<uint8_t>((high << 4) | low));
            }
            i = cursor - 1;
            continue;
        }

        if (introducer == ':' && escape >= '0' && escape <= '9') {
            uint8_t value = 0;
            if (!AppendNumericEscape(input, i, value, 10, 3, error)) return false;
            bytes.push_back(value);
            continue;
        }

        if (introducer == '\\' && escape == 'x') {
            uint8_t value = 0;
            ++i;
            if (!AppendNumericEscape(input, i, value, 16, 2, error)) return false;
            bytes.push_back(value);
            continue;
        }

        if (introducer == '\\' && escape == 'd') {
            uint8_t value = 0;
            ++i;
            if (!AppendNumericEscape(input, i, value, 10, 3, error)) return false;
            bytes.push_back(value);
            continue;
        }

        if (introducer == '\\' && escape >= '0' && escape <= '7') {
            size_t cursor = i + 1;
            unsigned value = 0;
            size_t digits = 0;
            while (cursor < input.size() && digits < 3 &&
                   input[cursor] >= '0' && input[cursor] <= '7') {
                value = value * 8 + static_cast<unsigned>(input[cursor] - '0');
                ++cursor;
                ++digits;
            }
            if (value > 255) {
                error = "octal byte escape is outside the range 0-255";
                return false;
            }
            bytes.push_back(static_cast<uint8_t>(value));
            i = cursor - 1;
            continue;
        }

        if (introducer == '\\') {
            switch (escape) {
                case 'a': bytes.push_back(0x07); break;
                case 'b': bytes.push_back(0x08); break;
                case 'e': bytes.push_back(0x1B); break;
                case 'f': bytes.push_back(0x0C); break;
                case 'n': bytes.push_back(0x0A); break;
                case 'r': bytes.push_back(0x0D); break;
                case 't': bytes.push_back(0x09); break;
                case 'v': bytes.push_back(0x0B); break;
                case '\\': bytes.push_back(static_cast<uint8_t>('\\')); break;
                default: bytes.push_back(static_cast<uint8_t>(escape)); break;
            }
            ++i;
            continue;
        }

        if (introducer == ':') {
            switch (escape) {
                case 'a': bytes.push_back(0x07); break;
                case 'b': bytes.push_back(0x08); break;
                case 'e': bytes.push_back(0x1B); break;
                case 'n': bytes.push_back(0x0A); break;
                case 'r': bytes.push_back(0x0D); break;
                case 't': bytes.push_back(0x09); break;
                case '0': bytes.push_back(0x00); break;
                default:
                    bytes.push_back(static_cast<uint8_t>(':'));
                    continue;
            }
            ++i;
        }
    }

    return true;
}
