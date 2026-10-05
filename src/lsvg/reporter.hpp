#pragma once

#include "lsvg.hpp"
#include "options.hpp"
#include <string>
#include <vector>

class LsvgReporter {
public:
    static std::string CsvEscape(const std::string& value);
    static std::string JsonEscape(const std::string& value);
    static void PrintVGDetail(const VolumeGroup& vg);
    static void PrintLVDetail(const VolumeGroup& vg);
    static void PrintPVDetail(const VolumeGroup& vg);
    static void ReportFormatted(const std::vector<VolumeGroup>& vgs, const CmdOptions& opts);
};
