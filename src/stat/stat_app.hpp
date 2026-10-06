#pragma once

#include "file_summary_report.hpp"
#include "stat_options.hpp"
#include "stat.hpp"

class FileSummaryApplication {
public:
    explicit FileSummaryApplication(ProgramOptions options);

    int run();

private:
    ProgramOptions m_opts;

    void render(const std::vector<FileSummaryReport>& reports);

    static void printVersion();

    static void printHelp();
};
