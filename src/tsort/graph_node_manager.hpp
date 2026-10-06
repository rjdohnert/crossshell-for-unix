#pragma once

#include "tsort.hpp"

class GraphNodeManager {
private:
    std::map<std::string, int> nameToId;
    std::vector<std::string> idToName;

public:
    int getOrCreateId(const std::string& name);

    size_t size() const;
    const std::string& getName(int id) const;
};
