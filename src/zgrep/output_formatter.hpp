#pragma once

#include "zgrep.hpp"

class OutputFormatter {
public:
    static std::string JsonQuote(const std::string& value);

    static std::string CsvQuote(const std::string& value);

    static void EmitRecords(std::istream& input, OutputFormat format, FILE* pipe);
};
