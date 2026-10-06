#pragma once

#include "supervisord.hpp"

void WriteServiceEvent(const std::string& message, WORD type = EVENTLOG_INFORMATION_TYPE);
