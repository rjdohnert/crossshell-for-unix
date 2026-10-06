#pragma once

#include "sudo.hpp"

int RunWorker(const std::wstring& guid, const std::wstring& targetCmd);

// Client Mode: Runs in the original user terminal window
