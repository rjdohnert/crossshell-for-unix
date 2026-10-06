#ifndef SEQUENCE_GENERATOR_HPP
#define SEQUENCE_GENERATOR_HPP

#include "seq.hpp"
#include "seq_options.hpp"

class SeqEngine {
private:
    SeqOptions options;

public:
    explicit SeqEngine(SeqOptions opts);
    int execute(std::ostream& out);
};

#endif // SEQUENCE_GENERATOR_HPP
