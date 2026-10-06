#include "sequence_generator.hpp"
#include "number_parser.hpp"
#include "number_formatter.hpp"

SeqEngine::SeqEngine(SeqOptions opts) : options(std::move(opts)) {}

int SeqEngine::execute(std::ostream& out) {
    try {
        NumberInfo nFirst = NumberParser::parse(options.startStr);
        NumberInfo nIncr  = NumberParser::parse(options.incrementStr);
        NumberInfo nLast  = NumberParser::parse(options.lastStr);

        double first = nFirst.value;
        double incr  = nIncr.value;
        double last  = nLast.value;

        if (incr == 0.0) {
            std::cerr << "seq: zero increment step\n";
            return 1;
        }

        if ((incr > 0 && first > last) || (incr < 0 && first < last)) {
            return 0;
        }

        int maxFrac = std::max({nFirst.fracWidth, nIncr.fracWidth, nLast.fracWidth});
        int maxInt  = std::max({nFirst.intWidth, nIncr.intWidth, nLast.intWidth});
        const double epsilon = std::max(1e-12, std::fabs(incr) * 1e-12);

        NumberFormatter formatter(options.customFormat, options.equalWidth, maxInt, maxFrac);

        bool wroteAny = false;
        double current = first;
        while ((incr > 0 && current <= last + epsilon) || (incr < 0 && current >= last - epsilon)) {
            if (wroteAny) {
                out << options.separator;
            }
            wroteAny = true;

            formatter.formatAndPrint(current, out);
            current += incr;
        }
        out << "\n";

    } catch (const std::exception&) {
        std::cerr << "seq: invalid floating point argument\n";
        return 1;
    }

    return 0;
}
