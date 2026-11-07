#pragma once

#include "handles_types.hpp"

#include <string>
#include <vector>

bool FormatProcesses(OutputFormat format,
                     const std::vector<ProcessHandleInfo>& processes,
                     std::string& output, std::wstring& error);
