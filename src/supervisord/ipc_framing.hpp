#pragma once

#include "supervisor_defaults.hpp"
#include "supervisord.hpp"

bool ReadExact(HANDLE h, char* buffer, DWORD bytesToRead);

bool WriteExact(HANDLE h, const char* buffer, DWORD bytesToWrite);

bool ReadIpcFrame(HANDLE h, std::string& payload);

bool WriteIpcFrame(HANDLE h, const std::string& payload);
