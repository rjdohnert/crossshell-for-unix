#ifndef TRANSCRIPT_WRITER_HPP
#define TRANSCRIPT_WRITER_HPP

#include "script.hpp"

class TranscriptWriter {
public:
    static bool writeAll(HANDLE handle, const void* data, DWORD size);
    static bool writeString(HANDLE handle, const std::string& text);
};

#endif // TRANSCRIPT_WRITER_HPP
