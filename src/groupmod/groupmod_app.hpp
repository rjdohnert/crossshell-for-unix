#pragma once

#include "groupmod_types.hpp"

class GroupModApplication {
public:
    explicit GroupModApplication(CommandOptions options);
    int Run();

private:
    CommandOptions m_options;

    int ProcessModification(const GroupModification& modification);
    int ProcessInput();
};
