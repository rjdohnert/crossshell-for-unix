/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "du_input_stream_reader.hpp"

namespace du {

bool ReadPaths(std::istream& input, char delimiter,
               std::vector<std::filesystem::path>& paths,
               std::string& error) {
    std::string path;
    while (std::getline(input, path, delimiter)) {
        if (delimiter == '\n' && !path.empty() && path.back() == '\r') {
            path.pop_back();
        }
        if (!path.empty()) paths.emplace_back(path);
    }

    if (input.bad()) {
        error = "Failed while reading path input.";
        return false;
    }
    return true;
}

} // namespace du
