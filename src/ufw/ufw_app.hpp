#pragma once

#include "firewall_manager.hpp"
#include "ufw.hpp"

class UfwApplication {
private:
    FirewallManager m_fw;

public:
    int Run(int argc, char* argv[]);
};
