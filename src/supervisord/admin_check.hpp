#pragma once

#include "supervisord.hpp"

bool IsTokenElevatedOrAdmin(HANDLE token);

bool IsCurrentProcessElevatedOrAdmin();
