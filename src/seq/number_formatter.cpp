#include "number_formatter.hpp"

NumberFormatter::NumberFormatter(std::string fmt, bool eqWidth, int maxInt, int maxFrac)
    : customFormat(std::move(fmt)), equalWidth(eqWidth), maxIntWidth(maxInt), maxFracWidth(maxFrac) {}

void NumberFormatter::formatAndPrint(double val, std::ostream& out) const {
    if (!customFormat.empty()) {
        char buf[256];
        snprintf(buf, sizeof(buf), customFormat.c_str(), val);
        out << buf;
    } else if (equalWidth) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(maxFracWidth) << val;
        std::string strVal = ss.str();

        bool isNeg = (val < 0);
        std::string absStr = isNeg ? strVal.substr(1) : strVal;

        int totalWidth = maxIntWidth + (maxFracWidth > 0 ? maxFracWidth + 1 : 0);
        int padLen = totalWidth - static_cast<int>(absStr.length());

        if (isNeg) out << "-";
        if (padLen > 0) out << std::string(padLen, '0');
        out << absStr;
    } else {
        if (maxFracWidth == 0) {
            out << static_cast<long long>(std::round(val));
        } else {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(maxFracWidth) << val;
            out << ss.str();
        }
    }
}
