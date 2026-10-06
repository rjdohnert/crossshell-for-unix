#pragma once

#include "supervisord.hpp"

enum class ProcessState { STOPPED, STARTING, RUNNING, BACKOFF, STOPPING, FATAL };

std::string StateToString(ProcessState state);
