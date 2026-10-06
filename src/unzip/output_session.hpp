#pragma once

#include "output_buffer.hpp"
#include "pipe_buffer.hpp"
#include "unzip.hpp"

class OutputSession { std::streambuf* old_;FILE*file_=nullptr;PipeBuffer*pipe_=nullptr;OutputBuffer*out_=nullptr;public:OutputSession(OutputFormat f,const std::string&cmd);~OutputSession();};
