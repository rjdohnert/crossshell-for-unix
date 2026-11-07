#ifndef DU_INPUT_STREAM_READER_HPP
#define DU_INPUT_STREAM_READER_HPP

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace du {

bool ReadPaths(std::istream& input, char delimiter,
               std::vector<std::filesystem::path>& paths,
               std::string& error);

} // namespace du

#endif
