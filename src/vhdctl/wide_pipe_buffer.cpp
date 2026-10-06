#include "wide_pipe_buffer.hpp"

WidePipeBuffer::int_type WidePipeBuffer::overflow(int_type ch)  {
        if (ch != traits_type::eof()) { *pptr() = static_cast<wchar_t>(ch); pbump(1); }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

int WidePipeBuffer::sync()  {
        std::ptrdiff_t count = pptr() - pbase();
        if (count > 0 && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), pipe_) != static_cast<size_t>(count)) return -1;
        setp(buffer_, buffer_ + sizeof(buffer_) / sizeof(buffer_[0]));
        return std::fflush(pipe_) == 0 ? 0 : -1;
    }

WidePipeBuffer::WidePipeBuffer(FILE* pipe) : pipe_(pipe) { setp(buffer_, buffer_ + sizeof(buffer_) / sizeof(buffer_[0])); }

WidePipeBuffer::~WidePipeBuffer()  { sync(); }
