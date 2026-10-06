#include "bison_output_session.hpp"
#include "bison_pipe_buffer.hpp"
#include "bison_structured_buffer.hpp"
#include "yacc_config.hpp"

BisonOutputSession::BisonOutputSession(Config::OutputFormat format, const std::string& command) : old_(std::cout.rdbuf()) {
        std::streambuf* target = old_;
        if (!command.empty()) {
            pipe_ = _popen(command.c_str(), "w");
            if (pipe_) { pipeBuffer_ = new BisonPipeBuffer(pipe_); target = pipeBuffer_; }
        }
        if (format != Config::OutputFormat::Human) {
            structured_ = new BisonStructuredBuffer(target, format);
            std::cout.rdbuf(structured_);
        } else if (pipe_) {
            std::cout.rdbuf(target);
        }
    }

BisonOutputSession::~BisonOutputSession() { std::cout.flush(); std::cout.rdbuf(old_); delete structured_; delete pipeBuffer_; if (pipe_) _pclose(pipe_); }
