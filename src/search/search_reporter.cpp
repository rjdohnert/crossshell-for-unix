#include "search_reporter.hpp"

void SearchReporter::PrintBanner() {
    std::cout << ConsoleTerminal::Bold << "search" << ConsoleTerminal::Reset 
              << " (C++17 Windows File Search Engine)\n"
              << "Run " << ConsoleTerminal::Yellow << "search --help" << ConsoleTerminal::Reset << " or " 
              << ConsoleTerminal::Yellow << "search /?" << ConsoleTerminal::Reset << " for full documentation and options.\n\n"
              << "Searching current directory recursively...\n\n";
}

void SearchReporter::PrintHeader(const SearchOptions& opt) {
    if (opt.bareOutput || opt.summaryOnly) return;

    std::cout << ConsoleTerminal::Bold << ConsoleTerminal::Cyan << "SEARCH TARGETS:" << ConsoleTerminal::Reset << "\n";
    for (const auto& p : opt.targetPaths) {
        std::error_code ec;
        std::cout << "  -> " << fs::absolute(p, ec).string() << "\n";
    }
    std::cout << "\n";

    std::cout << ConsoleTerminal::Dim 
              << std::left << std::setw(7)  << "TYPE"
              << std::left << std::setw(11) << "SIZE"
              << std::left << std::setw(8)  << "ATTR"
              << std::left << std::setw(21) << "MODIFIED"
              << "PATH" << ConsoleTerminal::Reset << "\n";
    std::cout << std::string(85, '-') << "\n";
}

void SearchReporter::PrintResult(const fs::directory_entry& entry, bool isDir, bool isReg, bool isSym,
                                uintmax_t fsize, const std::string& attrStr, fs::file_time_type ftime,
                                const SearchOptions& opt) {
    if (opt.bareOutput) {
        std::cout << entry.path().string() << "\n";
        return;
    }

    if (opt.summaryOnly) return;

    std::string typeBadge = ConsoleTerminal::FormatTypeBadge(isDir, isSym);
    std::string sizeStr = isDir ? "-" : SizeFormatter::Format(fsize);
    std::string timeStr = DateTimeFormatter::Format(ftime);

    std::cout << std::left << std::setw(16) << typeBadge
              << std::left << std::setw(11) << sizeStr
              << std::left << std::setw(8)  << attrStr
              << std::left << std::setw(21) << timeStr
              << (isDir ? ConsoleTerminal::Bold : "") << entry.path().string() << ConsoleTerminal::Reset
              << "\n";
}

void SearchReporter::PrintSummary(size_t totalScanned, size_t matchedFiles, size_t matchedDirs,
                                 uintmax_t matchedBytes, double durationMs, const SearchOptions& opt) {
    if (opt.bareOutput) return;

    std::cout << std::string(85, '-') << "\n"
              << ConsoleTerminal::Bold << "Summary: " << ConsoleTerminal::Reset
              << ConsoleTerminal::Green << matchedFiles << ConsoleTerminal::Reset << " files (" << SizeFormatter::Format(matchedBytes) << "), "
              << ConsoleTerminal::Blue << matchedDirs << ConsoleTerminal::Reset << " directories matched "
              << ConsoleTerminal::Dim << "(Scanned " << totalScanned << " items in " 
              << std::fixed << std::setprecision(1) << durationMs << " ms)" << ConsoleTerminal::Reset << "\n";
}
