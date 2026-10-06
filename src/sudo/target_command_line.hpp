#pragma once

#include "sudo.hpp"

std::wstring GetTargetCommandLine();

// Thread worker to forward data from Pipe/Handle to Handle/Pipe
