#ifndef REPORT_FORMATTER_HPP
#define REPORT_FORMATTER_HPP

#include "sar.hpp"

std::string GetCurrentTimestamp();
std::string GetSystemDate();
std::string GetHostNameString();
std::string WideToUtf8(const std::wstring& value);

void PrintHeaderLine();
std::string FormatPercent(double value);
std::string FormatKib(uint64_t kib);
std::string FormatRate(double value, const std::string& unit);
std::string FormatWaitMs(double ms);

void PrintSectionHeader(const std::string& title, const std::vector<std::string>& columns);
void PrintCsvSectionHeader(const std::string& title, const std::vector<std::string>& columns);
void PrintJsonSectionHeader(const std::string& title);
std::string JsonEscape(const std::string& value);

#endif // REPORT_FORMATTER_HPP
