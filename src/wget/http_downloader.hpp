#pragma once

#include "wget.hpp"

int download_http_file(std::string url, const std::string& filename, OutputFormat outputFormat, const std::string& pipeCommand);
