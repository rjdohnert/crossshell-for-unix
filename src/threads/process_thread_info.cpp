#include "process_thread_info.hpp"

std::string ProcessThreadInfo::narrowName() const {
        if (name.empty()) return "";
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, name.c_str(), (int)name.size(), nullptr, 0, nullptr, nullptr);
        std::string strTo(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, name.c_str(), (int)name.size(), &strTo[0], sizeNeeded, nullptr, nullptr);
        return strTo;
    }
