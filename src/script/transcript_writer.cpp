#include "transcript_writer.hpp"

bool TranscriptWriter::writeAll(HANDLE handle, const void* data, DWORD size) {
    const BYTE* bytes = static_cast<const BYTE*>(data);
    DWORD written = 0;
    while (size > 0) {
        if (!WriteFile(handle, bytes, size, &written, nullptr)) {
            return false;
        }
        bytes += written;
        size -= written;
    }
    return true;
}

bool TranscriptWriter::writeString(HANDLE handle, const std::string& text) {
    return writeAll(handle, text.data(), static_cast<DWORD>(text.size()));
}
