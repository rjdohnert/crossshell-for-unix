#pragma once

#include "software_item.hpp"
#include "swlist.hpp"

class OutputFormatter {
public:
    static void EmitHeader(const wstring& hostName);

    static void EmitVerbose(const vector<SoftwareItem>& items);

    static int EmitAttribute(const vector<SoftwareItem>& items, const wstring& attribute);

    static void EmitVendorGrouped(const vector<SoftwareItem>& items);

    static void EmitDefault(const vector<SoftwareItem>& items);
};
