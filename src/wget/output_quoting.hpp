#pragma once

#include "wget.hpp"

std::string JsonQuote(const std::string& value);
std::string CsvQuote(const std::string& value);

// Version: 2.0

// Helper function to extract a default filename from the URL if not provided
