#include "pipe_buffer.hpp"

PipeBuffer::PipeBuffer(FILE* file) : m_file(file) {
        setp(m_buffer, m_buffer + sizeof(m_buffer));
    }

PipeBuffer::int_type PipeBuffer::overflow(int_type ch)  {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

int PipeBuffer::sync()  {
        auto n = pptr() - pbase();
        if (n > 0) {
            std::fwrite(pbase(), 1, static_cast<size_t>(n), m_file);
        }
        setp(m_buffer, m_buffer + sizeof(m_buffer));
        return std::fflush(m_file);
    }
