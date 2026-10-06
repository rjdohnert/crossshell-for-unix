#pragma once

#include "zip.hpp"

struct ZipOptions {
    bool recursive = false;
    bool junk_paths = false;
    bool quiet = false;
    bool force = false;
    int compression_level = 6;
    std::string zip_filename;
    std::vector<std::string> input_paths;
    std::vector<std::string> exclude_paths;
    OutputFormat output_format = OutputFormat::Human;
    std::string pipe_command;
};
