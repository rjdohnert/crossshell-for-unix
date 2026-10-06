#pragma once

#include "tmux.hpp"
#include "wmux_engine.hpp"

std::string GetHomeDirectory();

std::string GetTmuxRcPath();

void EnsureTmuxRcExists();

void LoadTmuxRc(WmuxEngine& mux);
