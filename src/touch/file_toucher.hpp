#pragma once

#include "touch_options.hpp"
#include "touch.hpp"

class FileToucher {
public:
    bool Touch(const std::wstring& target, const TouchOptions& opts, const FILETIME& target_at, const FILETIME& target_mt) const;
};
