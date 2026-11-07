/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "du_size_formatter.hpp"

#include <iomanip>
#include <sstream>

namespace du {
namespace {

std::string FormatHumanReadable(std::uint64_t bytes) {
    constexpr std::uint64_t unit = 1024;
    constexpr const char* suffixes[] = {"K", "M", "G", "T", "P", "E"};
    if (bytes < unit) return std::to_string(bytes) + "B";

    double value = static_cast<double>(bytes);
    std::size_t exponent = 0;
    while (value >= static_cast<double>(unit) && exponent < 6) {
        value /= static_cast<double>(unit);
        ++exponent;
    }

    std::ostringstream output;
    if (value >= 10.0) {
        output << static_cast<std::uint64_t>(value + 0.5);
    } else {
        output << std::fixed << std::setprecision(1) << value;
    }
    output << suffixes[exponent - 1];
    return output.str();
}

} // namespace

std::string FormatSize(std::uint64_t bytes, const Options& options) {
    if (options.sizeFormat == SizeFormatMode::Bytes) {
        return std::to_string(bytes);
    }
    if (options.sizeFormat == SizeFormatMode::HumanReadable) {
        return FormatHumanReadable(bytes);
    }

    const std::uint64_t blockSize =
        static_cast<std::uint64_t>(options.blockSize);
    const std::uint64_t blocks = bytes / blockSize +
        (bytes % blockSize == 0 ? 0 : 1);
    return std::to_string(blocks);
}

} // namespace du
