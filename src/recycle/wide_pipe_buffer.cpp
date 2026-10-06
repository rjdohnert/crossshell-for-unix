#include "wide_pipe_buffer.hpp"

WidePipeBuffer::WidePipeBuffer(FILE* file) : m_file(file) {
        setp(m_buffer, m_buffer + 2048);
    }

WidePipeBuffer::int_type WidePipeBuffer::overflow(int_type ch)  {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<wchar_t>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

int WidePipeBuffer::sync()  {
        auto count = pptr() - pbase();
        if (count && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), m_file) != static_cast<size_t>(count)) {
            return -1;
        }
        setp(m_buffer, m_buffer + 2048);
        return std::fflush(m_file) == 0 ? 0 : -1;
    }
