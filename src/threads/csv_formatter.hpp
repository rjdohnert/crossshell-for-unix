#pragma once

#include "output_formatter.hpp"
#include "process_thread_info.hpp"
#include "threads.hpp"

class CsvFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessThreadInfo>& data) override;

private:
    static std::string escapeQuotes(std::string str);
};
