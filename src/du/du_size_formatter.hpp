#ifndef DU_SIZE_FORMATTER_HPP
#define DU_SIZE_FORMATTER_HPP

#include "du_models.hpp"

#include <cstdint>
#include <string>

namespace du {

std::string FormatSize(std::uint64_t bytes, const Options& options);

} // namespace du

#endif
