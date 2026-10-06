#include "compression_level.hpp"

std::string compression_level_to_ps(int level) {
    if (level <= 0) return "NoCompression";
    if (level <= 3) return "Fastest";
    return "Optimal";
}
