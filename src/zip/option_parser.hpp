#pragma once

#include "zip_options.hpp"
#include "zip.hpp"

int parse_zip_options(int argc, char* argv[], ZipOptions& opts, bool& exitEarly);
