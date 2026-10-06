#pragma once

#include "unzip.hpp"

class OutputBuffer : public std::streambuf { std::streambuf* target_; OutputFormat format_; std::string pending_; bool first_=true; void emit(); public: OutputBuffer(std::streambuf*t,OutputFormat f);~OutputBuffer()override;int_type overflow(int_type c)override;int sync()override;};
