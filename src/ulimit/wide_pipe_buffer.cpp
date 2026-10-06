#include "wide_pipe_buffer.hpp"

WidePipeBuffer::WidePipeBuffer(FILE* f) : file_(f) { setp(buffer_, buffer_ + 1024); }

WidePipeBuffer::int_type WidePipeBuffer::overflow(int_type c)  {
        if (c != traits_type::eof()) { *pptr() = static_cast<wchar_t>(c); pbump(1); }
        return sync() == 0 ? traits_type::not_eof(c) : traits_type::eof();
    }

int WidePipeBuffer::sync()  {
        auto n = pptr() - pbase();
        if (n) { fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(n), file_); }
        setp(buffer_, buffer_ + 1024);
        return fflush(file_);
    }
