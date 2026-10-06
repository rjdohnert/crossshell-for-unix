#include "typeset_pipe.hpp"

TypesetPipe::TypesetPipe(FILE* f) : m_file(f) {
        setp(m_buffer, m_buffer + 1024);
    }

TypesetPipe::int_type TypesetPipe::overflow(int_type c)  {
        if (c != traits_type::eof()) {
            *pptr() = static_cast<wchar_t>(c);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(c) : traits_type::eof();
    }

int TypesetPipe::sync()  {
        auto n = pptr() - pbase();
        if (n && m_file) {
            std::wstring value(pbase(), static_cast<size_t>(n));
            int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string utf8(static_cast<size_t>(size), '\0');
                WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &utf8[0], size, nullptr, nullptr);
                fwrite(utf8.data(), 1, utf8.size(), m_file);
            }
        }
        setp(m_buffer, m_buffer + 1024);
        return m_file ? fflush(m_file) : 0;
    }
