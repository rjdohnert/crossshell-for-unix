#pragma once

#include "pipe_buffer.hpp"
#include "session_data.hpp"
#include "w.hpp"

int reportUserSessions(const std::vector<SessionData>& userSessions, bool showHeader, bool shortFormat, bool showFrom, OutputFormat outputFormat);
