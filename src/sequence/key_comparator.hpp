#ifndef KEY_COMPARATOR_HPP
#define KEY_COMPARATOR_HPP

#include "sequence.hpp"
#include "sequence_options.hpp"
#include "field_extractor.hpp"

class ValueTransformer {
public:
    static std::string transform(std::string_view sv, const KeyFlags& flags);
};

class KeyComparator {
private:
    static const std::unordered_map<std::string, int> MONTH_MAP;

public:
    static int parseMonth(std::string_view s);
    static double parseHumanNumber(std::string_view s);
    static int compareVersion(std::string_view a, std::string_view b);
    static int compare(std::string_view a, std::string_view b, const KeyFlags& flags);
    static bool lineLess(const std::string& a, const std::string& b, const SequenceOptions& opt);
    static bool lineEqual(const std::string& a, const std::string& b, const SequenceOptions& opt);
};

#endif // KEY_COMPARATOR_HPP
