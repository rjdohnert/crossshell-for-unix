#pragma once

#include "unzip.hpp"

struct UnzipOptions {
    bool list = false;
    bool test = false;
    bool quiet = false;
    bool overwrite = false;
    bool never_overwrite = false;
    bool junk_paths = false;
    std::string dest_dir = ".";
    std::string zip_filename;
    std::vector<std::string> filters;
    OutputFormat output_format = OutputFormat::Human;
    std::string pipe_command;
};
