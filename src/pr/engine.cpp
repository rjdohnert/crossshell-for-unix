#include "engine.hpp"

// ============================================================================
// PaginationEngine Implementation
// ============================================================================

std::wstring PaginationEngine::getTimestampString() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_s(&tmv, &t);
    std::wstringstream ss;
    ss << std::put_time(&tmv, L"%b %d %H:%M %Y");
    return ss.str();
}

void PaginationEngine::paginateStream(std::wistream& in, const std::wstring& title, const PrOptions& opt, std::wostream& out) {
    std::vector<std::wstring> lines;
    for (std::wstring l; std::getline(in, l); ) {
        if (static_cast<int>(l.size()) > opt.width) {
            l = l.substr(0, opt.width);
        }
        lines.push_back(l);
    }

    int headerLines = opt.omitHeader ? 0 : 5;
    int contentLines = (std::max)(1, opt.pageLength - headerLines);
    int page = 1;

    for (size_t i = 0; i < lines.size(); i += contentLines, ++page) {
        if (!opt.omitHeader) {
            std::wstring docTitle = opt.header.empty() ? title : opt.header;
            out << L"\n" << getTimestampString() << L"  " << docTitle << L"  Page " << page << L"\n\n";
        }
        for (size_t j = i; j < (std::min)(lines.size(), i + static_cast<size_t>(contentLines)); ++j) {
            out << lines[j] << L"\n";
        }
        if (!opt.omitHeader) {
            out << L"\n\n\n";
        }
    }
}

// ============================================================================
// PrEngine Implementation
// ============================================================================

PrEngine::PrEngine(PrOptions opts) : options(std::move(opts)) {}

int PrEngine::execute() {
    std::wostringstream captured;
    std::wostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::wcout;

    for (const auto& f : options.files) {
        if (f == L"-") {
            PaginationEngine::paginateStream(std::wcin, L"standard input", options, *outStream);
        } else {
            std::wifstream in(f);
            if (!in) {
                std::wcerr << L"pr: cannot open " << f << L"\n";
                continue;
            }
            PaginationEngine::paginateStream(in, f, options, *outStream);
        }
    }

    if (options.outputFormat || !options.pipeCommand.empty()) {
        PrReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
    }

    return 0;
}
