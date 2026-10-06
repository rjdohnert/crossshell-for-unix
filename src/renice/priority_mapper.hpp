#pragma once

#include "renice.hpp"

class PriorityMapper {
public:
    static std::optional<DWORD> fromNiceValue(int nice) noexcept;

    static std::optional<DWORD> fromString(std::string_view name) noexcept;

    static std::string toString(DWORD priorityClass);
};
