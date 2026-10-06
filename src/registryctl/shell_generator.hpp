#pragma once

#include "registryctl.hpp"

class ShellGenerator {
public:
    static void GeneratePowerShell();

    static void GenerateZsh();

    static void GenerateKsh();

    static void GenerateCmdWrapper();
};
