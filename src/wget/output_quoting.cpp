#include "output_quoting.hpp"

std::string JsonQuote(const std::string& value) { std::string out = "\""; for (char ch : value) { if (ch == '"' || ch == '\\') out += '\\'; if (ch == '\n') out += 'n'; else if (ch == '\r') out += 'r'; else out += ch; } return out + "\""; }

std::string CsvQuote(const std::string& value) { std::string out = "\""; for (char ch : value) out += ch == '"' ? "\"\"" : std::string(1, ch); return out + "\""; }
