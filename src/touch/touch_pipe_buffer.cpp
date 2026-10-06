#include "touch_pipe_buffer.hpp"

TouchPipeBuffer::TouchPipeBuffer(FILE* file) : m_file(file) {
        setp(m_buffer, m_buffer + 1024);
    }

TouchPipeBuffer::int_type TouchPipeBuffer::overflow(int_type ch)  {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<wchar_t>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

int TouchPipeBuffer::sync()  {
        auto count = pptr() - pbase();
        if (count > 0) {
            std::wstring value(pbase(), static_cast<size_t>(count));
            int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string utf8(static_cast<size_t>(size), '\0');
                WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), utf8.data(), size, nullptr, nullptr);
                std::fwrite(utf8.data(), 1, utf8.size(), m_file);
            }
        }
        setp(m_buffer, m_buffer + 1024);
        return std::fflush(m_file);
    }
