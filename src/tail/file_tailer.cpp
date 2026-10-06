#include "file_tailer.hpp"
#include "tail_output.hpp"

bool FileTailer::TailLines(HANDLE hFile, long long count, bool from_start) {
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(hFile, &file_size)) return false;

        if (from_start) {
            LARGE_INTEGER zero = {};
            SetFilePointerEx(hFile, zero, nullptr, FILE_BEGIN);
            long long line_no = 1;
            std::vector<char> buf(64 * 1024);
            DWORD read_bytes = 0;
            std::string rem;
            while (ReadFile(hFile, buf.data(), static_cast<DWORD>(buf.size()), &read_bytes, nullptr) && read_bytes > 0) {
                size_t start = 0;
                for (size_t i = 0; i < read_bytes; ++i) {
                    if (buf[i] == '\n') {
                        if (line_no >= count) {
                            if (!rem.empty()) {
                                TailOutput::WriteBytes(rem.data(), rem.size());
                                rem.clear();
                            }
                            TailOutput::WriteBytes(buf.data() + start, i - start + 1);
                        }
                        line_no++;
                        start = i + 1;
                    }
                }
                if (start < read_bytes) {
                    if (line_no >= count) {
                        TailOutput::WriteBytes(buf.data() + start, read_bytes - start);
                    } else {
                        rem.append(buf.data() + start, read_bytes - start);
                    }
                }
            }
            return true;
        }

        if (count <= 0) {
            LARGE_INTEGER end_pos = {};
            SetFilePointerEx(hFile, end_pos, nullptr, FILE_END);
            return true;
        }

        const DWORD CHUNK_SIZE = 64 * 1024;
        std::vector<char> chunk(CHUNK_SIZE);
        long long lines_found = 0;
        long long curr_offset = file_size.QuadPart;
        LARGE_INTEGER target_pos = {};

        while (curr_offset > 0 && lines_found <= count) {
            DWORD to_read = static_cast<DWORD>((std::min)(static_cast<long long>(CHUNK_SIZE), curr_offset));
            curr_offset -= to_read;
            target_pos.QuadPart = curr_offset;
            SetFilePointerEx(hFile, target_pos, nullptr, FILE_BEGIN);

            DWORD read_bytes = 0;
            if (!ReadFile(hFile, chunk.data(), to_read, &read_bytes, nullptr) || read_bytes == 0) break;

            for (long long i = static_cast<long long>(read_bytes) - 1; i >= 0; --i) {
                if (chunk[static_cast<size_t>(i)] == '\n') {
                    if (curr_offset + i + 1 == file_size.QuadPart) continue;
                    lines_found++;
                    if (lines_found == count) {
                        target_pos.QuadPart = curr_offset + i + 1;
                        SetFilePointerEx(hFile, target_pos, nullptr, FILE_BEGIN);
                        goto read_forward;
                    }
                }
            }
        }

        target_pos.QuadPart = 0;
        SetFilePointerEx(hFile, target_pos, nullptr, FILE_BEGIN);

    read_forward:
        DWORD bytes_read = 0;
        while (ReadFile(hFile, chunk.data(), CHUNK_SIZE, &bytes_read, nullptr) && bytes_read > 0) {
            TailOutput::WriteBytes(chunk.data(), bytes_read);
        }
        return true;
    }

bool FileTailer::TailBytes(HANDLE hFile, long long count, bool from_start) {
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(hFile, &file_size)) return false;

        LARGE_INTEGER target_pos = {};
        if (from_start) {
            target_pos.QuadPart = (std::max)(0LL, count - 1);
        } else {
            target_pos.QuadPart = (std::max)(0LL, file_size.QuadPart - count);
        }
        SetFilePointerEx(hFile, target_pos, nullptr, FILE_BEGIN);

        const DWORD CHUNK_SIZE = 64 * 1024;
        std::vector<char> chunk(CHUNK_SIZE);
        DWORD bytes_read = 0;
        while (ReadFile(hFile, chunk.data(), CHUNK_SIZE, &bytes_read, nullptr) && bytes_read > 0) {
            TailOutput::WriteBytes(chunk.data(), bytes_read);
        }
        return true;
    }
