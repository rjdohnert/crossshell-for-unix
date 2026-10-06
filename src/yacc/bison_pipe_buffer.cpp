#include "bison_pipe_buffer.hpp"

BisonPipeBuffer::BisonPipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + sizeof(buffer_)); }

BisonPipeBuffer::int_type BisonPipeBuffer::overflow(int_type ch)  { if (ch != traits_type::eof()) { *pptr() = static_cast<char>(ch); pbump(1); } return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); }

int BisonPipeBuffer::sync()  { auto n = pptr() - pbase(); if (n && std::fwrite(pbase(), 1, static_cast<size_t>(n), file_) != static_cast<size_t>(n)) return -1; setp(buffer_, buffer_ + sizeof(buffer_)); return std::fflush(file_) == 0 ? 0 : -1; }
