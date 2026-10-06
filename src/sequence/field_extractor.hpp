#ifndef FIELD_EXTRACTOR_HPP
#define FIELD_EXTRACTOR_HPP

#include "sequence.hpp"

class FieldExtractor {
public:
    static std::string_view extractKey(const std::string& line, const KeyDefinition& key, char delimiter);
};

#endif // FIELD_EXTRACTOR_HPP
