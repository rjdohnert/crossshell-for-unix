#include "ipc_response.hpp"

IpcResponse IpcResponse::Ok(const std::string& code, const std::string& message ) {
        IpcResponse r;
        r.ok = true;
        r.code = code;
        r.message = message;
        return r;
    }

IpcResponse IpcResponse::OkWithPayload(const std::string& code, std::string payload) {
        IpcResponse r;
        r.ok = true;
        r.code = code;
        r.payload = std::move(payload);
        return r;
    }

IpcResponse IpcResponse::Err(const std::string& code, const std::string& message) {
        IpcResponse r;
        r.ok = false;
        r.code = code;
        r.message = message;
        return r;
    }

std::string IpcResponse::ToFrame() const {
        if (ok) {
            if (!payload.empty()) return "OK|" + code + "\n" + payload;
            if (!message.empty()) return "OK|" + code + "|" + message + "\n";
            return "OK|" + code + "\n";
        }
        return "ERR|" + code + "|" + message + "\n";
    }
