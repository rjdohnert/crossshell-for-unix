#pragma once

#include "wall.hpp"

class SessionBroadcaster {
public:
    static bool broadcastWts(HANDLE hServer, const std::string& title, const std::string& message, bool isGui);
};
