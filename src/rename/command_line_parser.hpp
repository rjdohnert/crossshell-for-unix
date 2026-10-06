#pragma once

#include "rename_options.hpp"
#include "rename.hpp"

class CommandLineParser {
public:
    static bool parse(int argc, char* argv[], RenameOptions& options);
};
