#pragma once

#include "groupmod_types.hpp"

#include <string>

GroupModResult ModifyLocalGroup(
    const std::wstring& groupName,
    const std::optional<std::wstring>& newName,
    const std::optional<std::wstring>& comment);
std::wstring GroupModErrorMessage(unsigned long status);
