#pragma once

#include "groupdel_types.hpp"

class GroupDelApplication {
public:
    explicit GroupDelApplication(CommandOptions options);
    int Run();

private:
    CommandOptions m_options;

    int ProcessSingleGroup(const std::wstring& name);
    int ProcessInput();
};
