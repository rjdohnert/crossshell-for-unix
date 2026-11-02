// String/encoding and interface-classification helpers.
#pragma once

#include "ipadm_common.h"
#include "ipadm_models.h"

std::string WStringToUtf8(const std::wstring& value);

// Escape a string for safe inclusion in JSON output.
std::string JsonEscape(const std::string& value);

// Escape a string for safe inclusion in CSV output.
std::string CsvEscape(const std::string& value);

// An interface is a "real" network interface if it is Ethernet/Wi-Fi/loopback
// and not a synthetic Windows pseudo-interface. Uses the shared filter from
// the models module to avoid a duplicated blocklist.
bool IsRealNetworkInterface(ULONG ifType, const std::string& friendlyName);
