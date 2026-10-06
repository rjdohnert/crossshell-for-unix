#include "pipe_buffer.hpp"

PipeBuffer::PipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + sizeof(buffer_)); }

PipeBuffer::int_type PipeBuffer::overflow(int_type ch)  { if (ch != traits_type::eof()) { *pptr() = static_cast<char>(ch); pbump(1); } return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); }

int PipeBuffer::sync()  { auto n = pptr() - pbase(); if (n) std::fwrite(pbase(), 1, static_cast<size_t>(n), file_); setp(buffer_, buffer_ + sizeof(buffer_)); return std::fflush(file_); }
