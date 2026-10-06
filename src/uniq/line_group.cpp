#include "line_group.hpp"

void LineGroup::reset(std::string firstLine) {
        representativeLine = firstLine;
        allLines.clear();
        allLines.push_back(std::move(firstLine));
        count = 1;
    }

void LineGroup::add(std::string line) {
        allLines.push_back(std::move(line));
        count++;
    }
