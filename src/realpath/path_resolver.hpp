#pragma once

#include "realpath_options.hpp"
#include "realpath.hpp"

class PathResolver {
public:
    static fs::path stripExtendedPrefix(const fs::path& p);

    static bool isSubpath(const fs::path& target, const fs::path& base);

    static bool resolve(const fs::path& input, const RealpathOptions& opts, fs::path& outPath, std::string& errMsg);

    static int execute(const RealpathOptions& opts);
};
