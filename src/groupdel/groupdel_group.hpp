#pragma once

#include "groupdel_types.hpp"

#include <string>

GroupDelResult DeleteLocalGroup(const std::wstring& name);
std::wstring GroupDelErrorMessage(unsigned long status);
