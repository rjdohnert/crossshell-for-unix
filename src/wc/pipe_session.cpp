#include "pipe_buffer.hpp"
#include "pipe_session.hpp"

PipeSession::PipeSession(const std::string& command) : old_(std::cout.rdbuf()) {
        if (!command.empty() && (file_ = _popen(command.c_str(), "w"))) {
            buffer_ = new PipeBuffer(file_);
            std::cout.rdbuf(buffer_);
        }
    }

PipeSession::~PipeSession() {
        std::cout.flush();
        std::cout.rdbuf(old_);
        delete buffer_;
        if (file_) _pclose(file_);
    }
