#pragma once

#include "whereis.hpp"

class LocationSearcher {
public:
    static std::vector<std::wstring> searchCategory(const std::wstring& target,
                                                    const std::vector<std::wstring>& dirs,
                                                    const std::vector<std::wstring>& exts);
};
