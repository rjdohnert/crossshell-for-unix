#pragma once

#include "sudo.hpp"

int RunElevatedDirect(const std::wstring& targetCmd);

// Worker Mode: Triggered post-UAC in hidden process
