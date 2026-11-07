#include "errpt_event_reader.hpp"

#include <algorithm>
#include <ctime>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace {

constexpr DWORD kInitialBufferSize = 64 * 1024;
constexpr DWORD kMaximumBufferSize = 16 * 1024 * 1024;
constexpr DWORD kRecordSignature = 0x654c664c;

std::string SeverityName(WORD type) {
    switch (type) {
        case EVENTLOG_ERROR_TYPE: return "ERROR";
        case EVENTLOG_WARNING_TYPE: return "WARN";
        case EVENTLOG_INFORMATION_TYPE: return "INFO";
        case EVENTLOG_AUDIT_SUCCESS: return "AUDIT_OK";
        case EVENTLOG_AUDIT_FAILURE: return "AUDIT_FAIL";
        default: return "OTHER";
    }
}

bool ConvertToUtf8(const std::wstring& value, std::string& converted,
                   std::wstring& error) {
    if (value.empty()) {
        converted.clear();
        return true;
    }
    if (value.size() >
        static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        error = L"an event-log string exceeds the supported size.";
        return false;
    }
    const int inputLength = static_cast<int>(value.size());
    const int outputLength = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), inputLength,
        nullptr, 0, nullptr, nullptr);
    if (outputLength <= 0) {
        error = L"event log contains an invalid UTF-16 string (Windows error " +
            std::to_wstring(GetLastError()) + L").";
        return false;
    }
    converted.resize(static_cast<std::size_t>(outputLength));
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            value.data(), inputLength, converted.data(),
                            outputLength, nullptr, nullptr) != outputLength) {
        error = L"cannot encode event-log text as UTF-8 (Windows error " +
            std::to_wstring(GetLastError()) + L").";
        return false;
    }
    return true;
}

bool FormatTime(DWORD timestamp, std::string& formatted,
                std::wstring& error) {
    const std::time_t value = static_cast<std::time_t>(timestamp);
    std::tm localTime{};
    if (localtime_s(&localTime, &value) != 0) {
        error = L"cannot convert an event timestamp.";
        return false;
    }
    char buffer[32]{};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                      &localTime) == 0) {
        error = L"cannot format an event timestamp.";
        return false;
    }
    formatted = buffer;
    return true;
}

bool ContainsRange(DWORD offset, DWORD length, DWORD recordLength) {
    return offset >= sizeof(EVENTLOGRECORD) &&
        offset <= recordLength && length <= recordLength - offset;
}

bool ReadWideString(const BYTE* record, DWORD recordLength, DWORD& offset,
                    std::wstring& value) {
    if (offset < sizeof(EVENTLOGRECORD) ||
        offset % alignof(wchar_t) != 0) {
        return false;
    }
    value.clear();
    while (offset <= recordLength &&
           recordLength - offset >= sizeof(wchar_t)) {
        wchar_t character = L'\0';
        std::memcpy(&character, record + offset, sizeof(character));
        offset += static_cast<DWORD>(sizeof(character));
        if (character == L'\0') return true;
        value.push_back(character);
    }
    return false;
}

bool ParseRecord(const BYTE* record, DWORD availableBytes,
                 EventEntry& entry, std::wstring& error) {
    if (availableBytes < sizeof(EVENTLOGRECORD)) {
        error = L"event log returned a truncated record header.";
        return false;
    }

    const auto* header = reinterpret_cast<const EVENTLOGRECORD*>(record);
    const DWORD recordLength = header->Length;
    if (header->Reserved != kRecordSignature ||
        recordLength < sizeof(EVENTLOGRECORD) ||
        recordLength % sizeof(DWORD) != 0 ||
        recordLength > availableBytes) {
        error = L"event log returned an invalid record length or signature.";
        return false;
    }

    DWORD trailingLength = 0;
    std::memcpy(&trailingLength,
                record + recordLength - sizeof(trailingLength),
                sizeof(trailingLength));
    if (trailingLength != recordLength) {
        error = L"event log record length markers do not match.";
        return false;
    }
    const DWORD payloadEnd = recordLength - sizeof(trailingLength);

    if ((header->UserSidLength != 0 &&
         !ContainsRange(header->UserSidOffset, header->UserSidLength,
                        payloadEnd)) ||
        (header->DataLength != 0 &&
         !ContainsRange(header->DataOffset, header->DataLength, payloadEnd))) {
        error = L"event log record contains an out-of-range data offset.";
        return false;
    }

    DWORD sourceEnd = payloadEnd;
    if (header->NumStrings != 0) {
        if (header->StringOffset < sizeof(EVENTLOGRECORD) ||
            header->StringOffset >= payloadEnd ||
            header->StringOffset % alignof(wchar_t) != 0) {
            error = L"event log record contains an invalid insertion-string offset.";
            return false;
        }
        sourceEnd = (std::min)(sourceEnd, header->StringOffset);
    }
    if (header->UserSidLength != 0 &&
        header->UserSidOffset >= sizeof(EVENTLOGRECORD)) {
        sourceEnd = (std::min)(sourceEnd, header->UserSidOffset);
    }
    if (header->DataLength != 0 &&
        header->DataOffset >= sizeof(EVENTLOGRECORD)) {
        sourceEnd = (std::min)(sourceEnd, header->DataOffset);
    }

    DWORD offset = static_cast<DWORD>(sizeof(EVENTLOGRECORD));
    std::wstring source;
    std::wstring computer;
    if (!ReadWideString(record, sourceEnd, offset, source) ||
        !ReadWideString(record, sourceEnd, offset, computer)) {
        error = L"event log record contains an unterminated source or computer name.";
        return false;
    }

    std::wstring summary;
    if (header->NumStrings != 0) {
        DWORD stringsEnd = payloadEnd;
        const auto limitStrings = [&](DWORD dataOffset, DWORD dataLength) {
            if (dataLength == 0) return true;
            const DWORD dataEnd = dataOffset + dataLength;
            if (dataOffset < header->StringOffset &&
                dataEnd > header->StringOffset) {
                return false;
            }
            if (dataOffset >= header->StringOffset) {
                stringsEnd = (std::min)(stringsEnd, dataOffset);
            }
            return true;
        };
        if (!limitStrings(header->UserSidOffset, header->UserSidLength) ||
            !limitStrings(header->DataOffset, header->DataLength) ||
            header->StringOffset < offset ||
            header->NumStrings >
                (stringsEnd - header->StringOffset) / sizeof(wchar_t)) {
            error = L"event log record contains overlapping or truncated "
                    L"insertion strings.";
            return false;
        }
        offset = header->StringOffset;
        for (DWORD index = 0; index < header->NumStrings; ++index) {
            std::wstring insertion;
            if (!ReadWideString(record, stringsEnd, offset, insertion)) {
                error = L"event log record contains an unterminated insertion string.";
                return false;
            }
            if (index == 0) summary = std::move(insertion);
        }
    }
    if (summary.empty()) summary = L"(no insertion text)";

    if (!ConvertToUtf8(source, entry.source, error) ||
        !ConvertToUtf8(summary, entry.summary, error) ||
        !FormatTime(header->TimeGenerated, entry.timestamp, error)) {
        return false;
    }
    entry.severity = SeverityName(header->EventType);
    entry.eventId = header->EventID & 0xffff;
    return true;
}

} // namespace

EventLogReader::~EventLogReader() {
    if (handle_) CloseEventLog(handle_);
}

bool EventLogReader::Open(const std::wstring& logName, std::wstring& error) {
    if (handle_) {
        error = L"an event log is already open.";
        return false;
    }
    handle_ = OpenEventLogW(nullptr, logName.c_str());
    if (handle_) return true;
    error = L"cannot open event log '" + logName + L"' (Windows error " +
        std::to_wstring(GetLastError()) + L").";
    return false;
}

bool EventLogReader::ReadEach(const ErrptOptions& options,
                              const Consumer& consume,
                              std::wstring& error) {
    if (!handle_) {
        error = L"no event log is open.";
        return false;
    }

    std::vector<BYTE> buffer(kInitialBufferSize);
    DWORD entriesRead = 0;
    const DWORD readFlags =
        EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ;

    for (;;) {
        DWORD bytesRead = 0;
        DWORD minimumBytes = 0;
        if (!ReadEventLogW(handle_, readFlags, 0, buffer.data(),
                           static_cast<DWORD>(buffer.size()), &bytesRead,
                           &minimumBytes)) {
            const DWORD readError = GetLastError();
            if (readError == ERROR_HANDLE_EOF ||
                readError == ERROR_SUCCESS) {
                return true;
            }
            if (readError == ERROR_INSUFFICIENT_BUFFER &&
                minimumBytes > buffer.size() &&
                minimumBytes <= kMaximumBufferSize) {
                buffer.resize(minimumBytes);
                continue;
            }
            error = L"failed while reading event log (Windows error " +
                std::to_wstring(readError) + L").";
            return false;
        }

        if (bytesRead == 0 || bytesRead > buffer.size()) {
            error = L"event log returned an invalid byte count.";
            return false;
        }

        DWORD offset = 0;
        while (offset < bytesRead) {
            EventEntry entry;
            const DWORD remaining = bytesRead - offset;
            if (!ParseRecord(buffer.data() + offset, remaining, entry, error)) {
                return false;
            }
            const auto* header = reinterpret_cast<const EVENTLOGRECORD*>(
                buffer.data() + offset);
            if (!consume(entry)) {
                error = L"failed while formatting event output.";
                return false;
            }
            ++entriesRead;
            if (!options.showAll && entriesRead >= options.maxEntries) {
                return true;
            }
            offset += header->Length;
        }
    }
}

bool EventLogReader::Close(std::wstring& error) {
    if (!handle_) return true;
    HANDLE handle = handle_;
    handle_ = nullptr;
    if (CloseEventLog(handle)) return true;
    error = L"cannot close the event log (Windows error " +
        std::to_wstring(GetLastError()) + L").";
    return false;
}
