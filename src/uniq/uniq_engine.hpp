#pragma once

#include "line_comparator.hpp"
#include "line_group.hpp"
#include "uniq_options.hpp"
#include "uniq.hpp"

class UniqEngine {
private:
    UniqOptions options;
    LineComparator comparator;
    char delimiter;
    bool hasEmittedFirstGroup{false};

public:
    explicit UniqEngine(UniqOptions opts);

    int execute();

private:
    void emitGroup(const LineGroup& group, std::ostream& out);
};
