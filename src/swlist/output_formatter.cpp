#include "output_formatter.hpp"
#include "software_item.hpp"
#include "system_info.hpp"

void OutputFormatter::EmitHeader(const wstring& hostName) {
        wcout << L"#\n"
              << L"# Target Selection Spec:\n"
              << L"#   Host: " << hostName << L"\n"
              << L"#\n";
    }

void OutputFormatter::EmitVerbose(const vector<SoftwareItem>& items) {
        wcout << left 
              << setw(35) << L"# Name"
              << setw(16) << L"Revision"
              << setw(22) << L"Vendor"
              << setw(12) << L"Date"
              << setw(8)  << L"Arch"
              << L"Location\n";
        wcout << wstring(115, L'=') << L"\n";

        for (const auto& item : items) {
            wcout << left
                  << setw(35) << item.name.substr(0, 34)
                  << setw(16) << item.revision.substr(0, 15)
                  << setw(22) << item.vendor.substr(0, 21)
                  << setw(12) << item.installDate.substr(0, 11)
                  << setw(8)  << item.arch
                  << item.location << L"\n";
        }
    }

int OutputFormatter::EmitAttribute(const vector<SoftwareItem>& items, const wstring& attribute) {
        if (attribute == L"TITLE") {
            wcout << L"# Name\n" << wstring(60, L'=') << L"\n";
            for (const auto& item : items) wcout << item.name << L"\n";
        } else if (attribute == L"REVISION" || attribute == L"VERSION") {
            wcout << left << setw(45) << L"# Name" << L"Revision\n" << wstring(65, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.revision << L"\n";
            }
        } else if (attribute == L"VENDOR" || attribute == L"PUBLISHER") {
            wcout << left << setw(45) << L"# Name" << L"Vendor\n" << wstring(75, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.vendor << L"\n";
            }
        } else if (attribute == L"DATE") {
            wcout << left << setw(45) << L"# Name" << L"Install Date\n" << wstring(65, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.installDate << L"\n";
            }
        } else if (attribute == L"ARCH" || attribute == L"ARCHITECTURE") {
            wcout << left << setw(45) << L"# Name" << L"Architecture\n" << wstring(60, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.arch << L"\n";
            }
        } else if (attribute == L"LOCATION" || attribute == L"PATH") {
            wcout << left << setw(45) << L"# Name" << L"Location\n" << wstring(85, L'=') << L"\n";
            for (const auto& item : items) {
                wcout << left << setw(45) << item.name.substr(0, 44) << item.location << L"\n";
            }
        } else {
            wcerr << L"Unknown attribute: " << attribute << L"\nUse -h for help.\n";
            return 1;
        }
        return 0;
    }

void OutputFormatter::EmitVendorGrouped(const vector<SoftwareItem>& items) {
        vector<SoftwareItem> vendorView = items;
        sort(vendorView.begin(), vendorView.end(), [](const SoftwareItem& a, const SoftwareItem& b) {
            const wstring va = SystemInfo::ToUpper(a.vendor);
            const wstring vb = SystemInfo::ToUpper(b.vendor);
            if (va != vb) return va < vb;
            return SystemInfo::ToUpper(a.name) < SystemInfo::ToUpper(b.name);
        });

        wstring currentVendor;
        for (const auto& item : vendorView) {
            if (currentVendor != item.vendor) {
                currentVendor = item.vendor;
                wcout << L"\n" << currentVendor << L"\n";
                wcout << wstring(currentVendor.length(), L'-') << L"\n";
            }
            wcout << left
                  << setw(42) << item.name.substr(0, 41)
                  << setw(18) << item.revision.substr(0, 17)
                  << item.arch << L"\n";
        }
    }

void OutputFormatter::EmitDefault(const vector<SoftwareItem>& items) {
        wcout << left 
              << setw(42) << L"# Name"
              << setw(18) << L"Revision"
              << L"Vendor / Title\n";
        wcout << wstring(85, L'=') << L"\n";

        for (const auto& item : items) {
            wcout << left
                  << setw(42) << item.name.substr(0, 41)
                  << setw(18) << item.revision.substr(0, 17)
                  << item.vendor << L"\n";
        }
    }
