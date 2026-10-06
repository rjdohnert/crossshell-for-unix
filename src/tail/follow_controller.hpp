#pragma once

#include "tail_options.hpp"
#include "tail.hpp"

class FollowController {
public:
    static void FollowFiles(const std::vector<std::wstring>& files, const TailOptions& opts, bool show_headers);
};
