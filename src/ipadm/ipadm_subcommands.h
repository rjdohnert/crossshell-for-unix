// ipadm subcommand implementations.
#pragma once

#include "ipadm_common.h"
#include "ipadm_models.h"

// Subcommand: show-if
void ShowIf(const std::vector<IpInterfaceInfo>& interfaces, const std::string& targetIf, const std::string& fmt = "table");

// Subcommand: show-addr
void ShowAddr(const std::vector<IpAddressObject>& addrs, const std::string& targetAddrObj, const std::string& fmt = "table");

// Subcommand: show-prop
void ShowProp(const std::string& targetProp, const std::string& targetProto);

// Subcommand: create-addr
bool CreateAddress(const std::string& type, const std::string& cidrAddr, const std::string& addrobj, const std::vector<IpInterfaceInfo>& interfaces);

// Subcommand: delete-addr
bool DeleteAddress(const std::string& addrobj, const std::vector<IpAddressObject>& addrs);
