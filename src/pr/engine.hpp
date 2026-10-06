#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "pr.hpp"
#include "options.hpp"
#include "reporter.hpp"

class PaginationEngine {
public:
    static std::wstring getTimestampString();
    static void paginateStream(std::wistream& in, const std::wstring& title, const PrOptions& opt, std::wostream& out);
};

class PrEngine {
private:
    PrOptions options;

public:
    explicit PrEngine(PrOptions opts);
    int execute();
};

#endif // ENGINE_HPP
