#pragma once

#include "supervisord.hpp"

std::vector<std::string> ParseIpcCommandTokens(const std::string& input);

std::string QuoteForIpc(const std::string& value);
