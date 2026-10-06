#include "follow_controller.hpp"
#include "tail_options.hpp"
#include "tail_output.hpp"

void FollowController::FollowFiles(const std::vector<std::wstring>& files, const TailOptions& opts, bool show_headers) {
        struct MonitoredFile {
            std::wstring path;
            HANDLE handle = INVALID_HANDLE_VALUE;
            LARGE_INTEGER last_size = {};
        };

        std::vector<MonitoredFile> targets;
        for (const auto& f : files) {
            if (f == L"-") continue;
            HANDLE h = CreateFileW(f.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            MonitoredFile mf;
            mf.path = f;
            mf.handle = h;
            if (h != INVALID_HANDLE_VALUE) {
                GetFileSizeEx(h, &mf.last_size);
            }
            targets.push_back(mf);
        }

        std::vector<char> read_buf(64 * 1024);
        std::wstring last_printed_file = L"";

        DWORD sleep_ms = static_cast<DWORD>(opts.sleep_interval_sec * 1000.0);
        if (sleep_ms == 0) sleep_ms = 100;

        while (true) {
            Sleep(sleep_ms);

            for (auto& tf : targets) {
                if (tf.handle == INVALID_HANDLE_VALUE && opts.retry) {
                    tf.handle = CreateFileW(tf.path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (tf.handle != INVALID_HANDLE_VALUE) {
                        tf.last_size.QuadPart = 0;
                    }
                }

                if (tf.handle == INVALID_HANDLE_VALUE) continue;

                LARGE_INTEGER current_size = {};
                if (!GetFileSizeEx(tf.handle, &current_size)) {
                    CloseHandle(tf.handle);
                    tf.handle = INVALID_HANDLE_VALUE;
                    continue;
                }

                if (current_size.QuadPart < tf.last_size.QuadPart) {
                    std::string note = "tail: " + TailOutput::Utf8(tf.path) + ": file truncated\n";
                    TailOutput::WriteBytes(note.data(), note.size());
                    LARGE_INTEGER zero = {};
                    SetFilePointerEx(tf.handle, zero, nullptr, FILE_BEGIN);
                    tf.last_size.QuadPart = 0;
                }

                if (current_size.QuadPart > tf.last_size.QuadPart) {
                    if (show_headers && last_printed_file != tf.path) {
                        bool dummy = false;
                        TailOutput::PrintHeader(tf.path, dummy);
                        last_printed_file = tf.path;
                    }

                    DWORD bytes_read = 0;
                    while (ReadFile(tf.handle, read_buf.data(), static_cast<DWORD>(read_buf.size()), &bytes_read, nullptr) && bytes_read > 0) {
                        TailOutput::WriteBytes(read_buf.data(), bytes_read);
                    }
                    GetFileSizeEx(tf.handle, &tf.last_size);
                }
            }
        }
    }
