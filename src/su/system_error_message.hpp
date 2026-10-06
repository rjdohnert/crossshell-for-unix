#pragma once

#include "su.hpp"

std::wstring GetSystemErrorMessage(DWORD errorCode);

// Securely reads password from terminal without echoing characters
