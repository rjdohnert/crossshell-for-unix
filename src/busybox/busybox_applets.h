#pragma once

// Applet dispatch for the busybox multi-call binary.

#include "busybox.h"
#include "busybox_runtime.h"

using AppletFunc = std::function<int(const std::vector<std::string>&, ShellContext&)>;
extern std::map<std::string, AppletFunc> applets;

int cmd_sh(const std::vector<std::string>& args, ShellContext& ctx);
int cmd_help(const std::vector<std::string>& args, ShellContext& ctx);
