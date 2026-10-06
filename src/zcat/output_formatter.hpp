#pragma once

#include "zcat.hpp"

class OutputFormatter {
public:
    static std::string QuoteJson(const std::string& value);

    static std::string QuoteCsv(const std::string& value);

    static void EmitData(const std::string& data, OutputFormat format, std::ostream& out);
};
