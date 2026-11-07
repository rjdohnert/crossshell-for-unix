#pragma once

#include "gzip_types.hpp"

#include <string>

class GzipReport {
public:
    static bool dispatch(const std::vector<GzipOperation>& operations,
                         ReportFormat format, const std::wstring& pipeCommand,
                         bool stderrOutput, std::wstring& error);
};
