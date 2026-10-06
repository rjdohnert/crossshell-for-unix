#pragma once

#include "unzip_options.hpp"
#include "unzip.hpp"

#include <optional>

std::optional<int> parseUnzipOptions(int argc, char* argv[], UnzipOptions& opts);
