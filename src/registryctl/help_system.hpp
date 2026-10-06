#pragma once

#include "registryctl.hpp"

class HelpSystem {
public:
    static void PrintGeneralHelp();

    static void PrintTopicHelp(const std::string& topic);
};
