#pragma once

#include "output_formatter.hpp"
#include "process_thread_info.hpp"
#include "threads.hpp"

class JsonFormatter : public IOutputFormatter {
public:
    void format(std::ostream& os, const std::vector<ProcessThreadInfo>& data) override;

private:
    static std::string escapeJson(const std::string& str);
};
