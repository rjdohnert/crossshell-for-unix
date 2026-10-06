#include "file_analyzer.hpp"
#include "file_summary_report.hpp"
#include "pipeline_manager.hpp"
#include "report_formatter.hpp"
#include "stat_app.hpp"
#include "stat_options.hpp"

FileSummaryApplication::FileSummaryApplication(ProgramOptions options)
        : m_opts(std::move(options)) {}

int FileSummaryApplication::run() {
        if (m_opts.showHelp) {
            printHelp();
            return 0;
        }

        if (m_opts.showVersion) {
            printVersion();
            return 0;
        }

        // Support for piping file paths through stdin
        if (m_opts.targetPaths.empty() && PipelineManager::isInputPiped()) {
            auto piped = PipelineManager::readPipedPaths();
            m_opts.targetPaths.insert(m_opts.targetPaths.end(), piped.begin(), piped.end());
        }

        if (m_opts.targetPaths.empty()) {
            std::wcerr << L"stat: error: no files specified.\n";
            std::wcerr << L"Try 'stat --help' or 'stat -?' for more information.\n";
            return 1;
        }

        std::vector<FileSummaryReport> reports;
        for (const auto& path : m_opts.targetPaths) {
            reports.push_back(FileAnalyzer::analyzeFile(path));
        }

        render(reports);
        return 0;
    }

void FileSummaryApplication::render(const std::vector<FileSummaryReport>& reports) {
        if (m_opts.format == OutputFormat::JSON) {
            ReportFormatter::renderJSON(std::wcout, reports);
        } else if (m_opts.format == OutputFormat::CSV) {
            ReportFormatter::renderCSV(std::wcout, reports);
        } else if (m_opts.format == OutputFormat::Detailed || (m_opts.format == OutputFormat::Auto && reports.size() == 1)) {
            for (const auto& r : reports) {
                ReportFormatter::renderDetailed(std::wcout, r, m_opts.humanReadable);
            }
        } else {
            ReportFormatter::renderTable(std::wcout, reports, m_opts.humanReadable);
        }
    }

void FileSummaryApplication::printVersion() {
        std::wcout << L"stat version 1.5.0\n";
        std::wcout << L"Copyright (C) 2026, Roberto J Dohnert\n";
    }

void FileSummaryApplication::printHelp() {
        std::wcout << LR"(stat(1)                 CrossShell for UNIX Reference Manual                  stat(1)

    NAME
        stat - display file status and classification information

    SYNOPSIS
        stat [OPTIONS] [FILE]...

    DESCRIPTION
        Displays file classifications, text line counts, Windows owner SIDs,
        and creation timestamps for each FILE. If no FILE arguments are specified,
        stat automatically reads whitespace- or newline-delimited file paths from
        standard input (stdin). Clean stream formatting allows direct piping into
        findstr, Select-String, jq, or downstream tools.

    OPTIONS
        -d, --detailed
            Display full detailed property cards for all files.

        -t, --table
            Force tabular display format.

        -b, --bytes
            Print exact byte counts instead of human-readable sizes.

        --json
            Emit results in JSON format for automated pipelines.

        --csv
            Emit results in CSV format for spreadsheets and data parsers.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        stat main.cpp
            Display detailed summary card for a single file.

        stat *.cpp *.h
            Display tabular summary of all C++ source files.

        stat --json app.log
            Output metadata in JSON structure.

        dir /b /s *.txt | stat
            Pipe directory listings directly into stat.

    CrossShell for UNIX                                                     stat(1)
    )";
    }
