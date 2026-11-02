// IP Helper Enumerator Engine
#pragma once

#include "ipadm_common.h"
#include "ipadm_models.h"

class IpAdminEngine {
public:
    static std::vector<IpInterfaceInfo> DiscoverInterfaces();
    static std::vector<IpAddressObject> DiscoverAddressObjects(const std::vector<IpInterfaceInfo>& interfaces);
};
