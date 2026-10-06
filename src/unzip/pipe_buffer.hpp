#pragma once

#include "unzip.hpp"

class PipeBuffer : public std::streambuf { FILE* file_; char buffer_[4096]; public: explicit PipeBuffer(FILE*f);int_type overflow(int_type c)override;int sync()override;};
