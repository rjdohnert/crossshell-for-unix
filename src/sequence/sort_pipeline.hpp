#ifndef SORT_PIPELINE_HPP
#define SORT_PIPELINE_HPP

#include "sequence.hpp"
#include "sequence_options.hpp"
#include "key_comparator.hpp"

class SequenceEngine {
private:
    SequenceOptions options;

public:
    explicit SequenceEngine(SequenceOptions opts);
    int execute();
};

#endif // SORT_PIPELINE_HPP
