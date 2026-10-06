#pragma once

#include "supervisord.hpp"

struct IpcResponse {
    bool ok = false;
    std::string code;
    std::string message;
    std::string payload;

    static IpcResponse Ok(const std::string& code, const std::string& message = std::string());

    static IpcResponse OkWithPayload(const std::string& code, std::string payload);

    static IpcResponse Err(const std::string& code, const std::string& message);

    std::string ToFrame() const;
};
