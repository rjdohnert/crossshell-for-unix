#ifndef NL_ENGINE_HPP
#define NL_ENGINE_HPP

#include "nl.hpp"
#include "options.hpp"

class LineNumberFormatter {
public:
    static std::string format(long long num, int width, NumberFormat fmt);
};

class NlEngine {
private:
    NlOptions options;
    static bool shouldNumber(const std::string& line, const NlStyle& style, int& blankCount, int blankLimit);
    void processStream(std::istream& in) const;

public:
    explicit NlEngine(NlOptions opts);
    int execute();
};

#endif // NL_ENGINE_HPP
