#pragma once

#include "software_item.hpp"
#include "swlist.hpp"

class SoftwareCollector {
public:
    static vector<SoftwareItem> CollectInstalledSoftware();

    static vector<SoftwareItem> Filter(const vector<SoftwareItem>& allSoftware, const vector<wstring>& searchPatterns, bool showSystemComponents);
};
