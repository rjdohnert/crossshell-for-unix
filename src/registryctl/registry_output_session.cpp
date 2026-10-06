#include "registry_line_formatter.hpp"
#include "registry_output_session.hpp"
#include "registry_pipe_buffer.hpp"

RegistryOutputSession::RegistryOutputSession(RegistryOutputFormat format, const std::string& command) : old_(std::cout.rdbuf()) {
        std::streambuf* target = old_;
        if (!command.empty()) {
            pipe_ = _popen(command.c_str(), "w");
            if (pipe_) { pipeBuffer_ = new RegistryPipeBuffer(pipe_); target = pipeBuffer_; }
        }
        if (format == RegistryOutputFormat::Csv || format == RegistryOutputFormat::Table) {
            formatter_ = new RegistryLineFormatter(target, format);
            std::cout.rdbuf(formatter_);
        } else {
            std::cout.rdbuf(target);
        }
    }

RegistryOutputSession::~RegistryOutputSession() { std::cout.flush(); std::cout.rdbuf(old_); delete formatter_; delete pipeBuffer_; if (pipe_) _pclose(pipe_); }
