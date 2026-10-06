#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <string>
#include <vector>

class MachinfoOptions {
public:
    bool verbose{false};
    bool quiet{false};
    bool tpmWmi{false};
    bool showBios{false};
    bool showCpu{false};
    bool showMemory{false};
    bool showTpm{false};
    bool showAll{false};
    enum class OutputFormat { Table, Csv, Json } format{OutputFormat::Table};
    std::vector<std::wstring> filters;

    static void printHelp(const wchar_t* progName = L"machinfo");
    static bool parse(int argc, wchar_t* argv[], MachinfoOptions& opts);
};

#endif // OPTIONS_HPP
