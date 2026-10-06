#include "ipc_action_parser.hpp"
#include "supervisor_defaults.hpp"

IpcAction ParseIpcAction(const std::string& action) {
    if (action == "status") return IpcAction::Status;
    if (action == "diag") return IpcAction::Diag;
    if (action == "metrics") return IpcAction::Metrics;
    if (action == "reload") return IpcAction::Reload;
    if (action == "start") return IpcAction::Start;
    if (action == "stop") return IpcAction::Stop;
    if (action == "restart") return IpcAction::Restart;
    return IpcAction::Unknown;
}
