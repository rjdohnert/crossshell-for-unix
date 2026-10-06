#include "graph_node_manager.hpp"

int GraphNodeManager::getOrCreateId(const std::string& name) {
        auto it = nameToId.find(name);
        if (it != nameToId.end()) {
            return it->second;
        }
        int newId = static_cast<int>(idToName.size());
        nameToId[name] = newId;
        idToName.push_back(name);
        return newId;
    }

size_t GraphNodeManager::size() const { return idToName.size(); }

const std::string& GraphNodeManager::getName(int id) const { return idToName[id]; }
