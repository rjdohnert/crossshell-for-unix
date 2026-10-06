#include "key_comparator.hpp"

const std::unordered_map<std::string, int> KeyComparator::MONTH_MAP = {
    {"JAN", 1}, {"FEB", 2}, {"MAR", 3}, {"APR", 4}, {"MAY", 5}, {"JUN", 6},
    {"JUL", 7}, {"AUG", 8}, {"SEP", 9}, {"OCT", 10}, {"NOV", 11}, {"DEC", 12}
};

std::string ValueTransformer::transform(std::string_view sv, const KeyFlags& flags) {
    std::string res;
    res.reserve(sv.size());
    size_t start = 0;
    if (flags.ignoreLeadingBlanks) {
        while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) ++start;
    }
    for (size_t i = start; i < sv.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        if (flags.dictOrder && !std::isalnum(c) && !std::isspace(c)) continue;
        if (flags.ignoreNonPrint && !std::isprint(c)) continue;
        if (flags.ignoreCase) c = std::tolower(c);
        res.push_back(static_cast<char>(c));
    }
    return res;
}

int KeyComparator::parseMonth(std::string_view s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i + 3 <= s.size()) {
        std::string m = "";
        for (int k = 0; k < 3; ++k) m += std::toupper(static_cast<unsigned char>(s[i + k]));
        auto it = MONTH_MAP.find(m);
        if (it != MONTH_MAP.end()) return it->second;
    }
    return 0;
}

double KeyComparator::parseHumanNumber(std::string_view s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    char* endptr = nullptr;
    std::string str(s.substr(i));
    double val = std::strtod(str.c_str(), &endptr);
    if (endptr && *endptr) {
        char unit = std::toupper(static_cast<unsigned char>(*endptr));
        switch (unit) {
            case 'K': val *= 1024.0; break;
            case 'M': val *= 1024.0 * 1024.0; break;
            case 'G': val *= 1024.0 * 1024.0 * 1024.0; break;
            case 'T': val *= 1024.0 * 1024.0 * 1024.0 * 1024.0; break;
            case 'P': val *= 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0; break;
            case 'E': val *= 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0; break;
            default: break;
        }
    }
    return val;
}

int KeyComparator::compareVersion(std::string_view a, std::string_view b) {
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        if (i < a.size() && j < b.size() && std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j]))) {
            while (i < a.size() && a[i] == '0') ++i;
            while (j < b.size() && b[j] == '0') ++j;
            size_t startI = i, startJ = j;
            while (i < a.size() && std::isdigit(static_cast<unsigned char>(a[i]))) ++i;
            while (j < b.size() && std::isdigit(static_cast<unsigned char>(b[j]))) ++j;
            size_t lenA = i - startI, lenB = j - startJ;
            if (lenA != lenB) return (lenA < lenB) ? -1 : 1;
            int cmp = a.substr(startI, lenA).compare(b.substr(startJ, lenB));
            if (cmp != 0) return cmp;
        } else {
            char ca = (i < a.size()) ? a[i] : 0;
            char cb = (j < b.size()) ? b[j] : 0;
            if (ca != cb) return (ca < cb) ? -1 : 1;
            if (i < a.size()) ++i;
            if (j < b.size()) ++j;
        }
    }
    return 0;
}

int KeyComparator::compare(std::string_view a, std::string_view b, const KeyFlags& flags) {
    int cmp = 0;
    if (flags.numeric) {
        char* ea = nullptr; char* eb = nullptr;
        std::string sa(a), sb(b);
        long double va = std::strtold(sa.c_str(), &ea);
        long double vb = std::strtold(sb.c_str(), &eb);
        if (va < vb) cmp = -1;
        else if (va > vb) cmp = 1;
        else cmp = 0;
    } else if (flags.generalNumeric) {
        char* ea = nullptr; char* eb = nullptr;
        std::string sa(a), sb(b);
        double va = std::strtod(sa.c_str(), &ea);
        double vb = std::strtod(sb.c_str(), &eb);
        if (va < vb) cmp = -1;
        else if (va > vb) cmp = 1;
        else cmp = 0;
    } else if (flags.humanNumeric) {
        double va = parseHumanNumber(a);
        double vb = parseHumanNumber(b);
        if (va < vb) cmp = -1;
        else if (va > vb) cmp = 1;
        else cmp = 0;
    } else if (flags.month) {
        int ma = parseMonth(a);
        int mb = parseMonth(b);
        cmp = (ma < mb) ? -1 : (ma > mb ? 1 : 0);
    } else if (flags.version) {
        cmp = compareVersion(a, b);
    } else {
        std::string ta = ValueTransformer::transform(a, flags);
        std::string tb = ValueTransformer::transform(b, flags);
        cmp = ta.compare(tb);
    }
    return flags.reverse ? -cmp : cmp;
}

bool KeyComparator::lineLess(const std::string& a, const std::string& b, const SequenceOptions& opt) {
    if (!opt.keys.empty()) {
        for (const auto& k : opt.keys) {
            std::string_view ka = FieldExtractor::extractKey(a, k, opt.delimiter);
            std::string_view kb = FieldExtractor::extractKey(b, k, opt.delimiter);
            const KeyFlags& f = k.hasCustomFlags ? k.flags : opt.globalFlags;
            int c = compare(ka, kb, f);
            if (c != 0) return c < 0;
        }
    }
    int fallback = compare(a, b, opt.globalFlags);
    return fallback < 0;
}

bool KeyComparator::lineEqual(const std::string& a, const std::string& b, const SequenceOptions& opt) {
    return !lineLess(a, b, opt) && !lineLess(b, a, opt);
}
