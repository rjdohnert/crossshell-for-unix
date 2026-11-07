#pragma once

#include "groupadd_types.hpp"

class GroupAddApplication {
public:
    explicit GroupAddApplication(CommandOptions options);
    int Run();

private:
    CommandOptions m_options;

    int ProcessSingleGroup(const GroupRequest& request);
    int ProcessInput();
};
