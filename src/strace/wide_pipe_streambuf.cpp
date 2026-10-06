#include "wide_pipe_streambuf.hpp"

WidePipeStreambuf::WidePipeStreambuf(FILE* pipe) : m_pipe(pipe) {
    setp(m_buffer, m_buffer + (sizeof(m_buffer) / sizeof(m_buffer[0])));
}

WidePipeStreambuf::~WidePipeStreambuf() {
    sync();
}

std::wstreambuf::int_type WidePipeStreambuf::overflow(int_type ch) {
    if (ch != traits_type::eof()) {
        *pptr() = static_cast<wchar_t>(ch);
        pbump(1);
    }
    return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
}

int WidePipeStreambuf::sync() {
    std::ptrdiff_t count = pptr() - pbase();
    if (count > 0 && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), m_pipe) != static_cast<size_t>(count)) {
        return -1;
    }
    setp(m_buffer, m_buffer + (sizeof(m_buffer) / sizeof(m_buffer[0])));
    return std::fflush(m_pipe) == 0 ? 0 : -1;
}
