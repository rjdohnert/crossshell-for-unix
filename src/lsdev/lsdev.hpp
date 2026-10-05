#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <string>
#include <vector>

namespace StringUtil {
    std::string ToLower(const std::string& str);
    std::string WideToAnsi(const std::wstring& wstr);
    std::vector<std::string> Split(const std::string& str, char delim);
}

class Device {
public:
    Device(std::string logicalName,
           std::string state,
           std::string location,
           std::string description,
           std::string devClass,
           std::string subClass,
           std::string devType);

    const std::string& GetName() const noexcept;
    const std::string& GetState() const noexcept;
    const std::string& GetLocation() const noexcept;
    const std::string& GetDescription() const noexcept;
    const std::string& GetClass() const noexcept;
    const std::string& GetSubClass() const noexcept;
    const std::string& GetType() const noexcept;
    std::string GetField(const std::string& fieldName) const;

private:
    std::string m_logicalName;
    std::string m_state;
    std::string m_location;
    std::string m_description;
    std::string m_class;
    std::string m_subClass;
    std::string m_type;
};
