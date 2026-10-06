#ifndef NUMBER_FORMATTER_HPP
#define NUMBER_FORMATTER_HPP

#include "seq.hpp"

class NumberFormatter {
private:
    std::string customFormat;
    bool equalWidth;
    int maxIntWidth;
    int maxFracWidth;

public:
    NumberFormatter(std::string fmt, bool eqWidth, int maxInt, int maxFrac);
    void formatAndPrint(double val, std::ostream& out) const;
};

#endif // NUMBER_FORMATTER_HPP
