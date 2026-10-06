#include "registry_pipe_buffer.hpp"

RegistryPipeBuffer::RegistryPipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + sizeof(buffer_)); }

RegistryPipeBuffer::int_type RegistryPipeBuffer::overflow(int_type ch)  { if (ch != traits_type::eof()) { *pptr() = static_cast<char>(ch); pbump(1); } return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); }

int RegistryPipeBuffer::sync()  { auto count = pptr() - pbase(); if (count && std::fwrite(pbase(), 1, static_cast<size_t>(count), file_) != static_cast<size_t>(count)) return -1; setp(buffer_, buffer_ + sizeof(buffer_)); return std::fflush(file_) == 0 ? 0 : -1; }
