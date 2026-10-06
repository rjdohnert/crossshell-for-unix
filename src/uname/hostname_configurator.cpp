#include "hostname_configurator.hpp"

bool HostnameConfigurator::setHostname(const std::wstring& newName) {
        return SetComputerNameExW(ComputerNamePhysicalDnsHostname, newName.c_str()) != 0;
    }
