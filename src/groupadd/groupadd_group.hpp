#pragma once

#include "groupadd_types.hpp"

#include <string>

GroupAddResult CreateLocalGroup(const std::wstring& name,
                                const std::wstring& comment);
std::wstring GroupAddErrorMessage(const GroupAddResult& result);
