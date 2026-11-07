#ifndef ERRPT_EVENT_READER_HPP
#define ERRPT_EVENT_READER_HPP

#include "errpt_event.hpp"
#include "errpt_options.hpp"

#include <windows.h>

#include <functional>
#include <string>

class EventLogReader {
public:
    using Consumer = std::function<bool(const EventEntry&)>;

    EventLogReader() = default;
    EventLogReader(const EventLogReader&) = delete;
    EventLogReader& operator=(const EventLogReader&) = delete;
    ~EventLogReader();

    bool Open(const std::wstring& logName, std::wstring& error);
    bool ReadEach(const ErrptOptions& options, const Consumer& consume,
                  std::wstring& error);
    bool Close(std::wstring& error);

private:
    HANDLE handle_ = nullptr;
};

#endif
