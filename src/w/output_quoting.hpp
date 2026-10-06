#pragma once

#include "string_encoding.hpp"
#include "w.hpp"

std::string CsvQuote(const std::string& value);
std::string JsonQuote(const std::string& value);

// Convert Wide String to UTF-8
