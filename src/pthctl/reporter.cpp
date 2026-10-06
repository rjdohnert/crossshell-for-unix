#include "reporter.hpp"

// PthOutputBuffer
PthOutputBuffer::PthOutputBuffer(std::wstreambuf* target, OutputFormat format) 
    : target_(target), format_(format) {}

void PthOutputBuffer::write(const std::wstring& value) { 
    target_->sputn(value.data(), static_cast<std::streamsize>(value.size())); 
}

void PthOutputBuffer::emit() {
    if (pending_.empty()) return;
    std::wstring formatted;
    if (format_ == OutputFormat::Json) {
        if (!first_) write(L",\n");
        first_ = false;
        formatted = L"{\"message\":\"";
        for (wchar_t ch : pending_) { 
            if (ch == L'"') formatted += L"\\\""; 
            else if (ch == L'\\') formatted += L"\\\\"; 
            else if (ch == L'\r') formatted += L"\\r"; 
            else formatted += ch; 
        }
        formatted += L"\"}";
    } else if (format_ == OutputFormat::Csv) {
        formatted = L"\"";
        for (wchar_t ch : pending_) { 
            if (ch == L'"') formatted += L"\"\""; 
            else formatted += ch; 
        }
        formatted += L"\"\n";
    } else if (format_ == OutputFormat::Table) {
        formatted = pending_ + L"\n";
    } else {
        formatted = pending_ + L"\n";
    }
    write(formatted);
    pending_.clear();
}

std::wstreambuf::int_type PthOutputBuffer::overflow(int_type ch) {
    if (ch != traits_type::eof()) { 
        pending_.push_back(static_cast<wchar_t>(ch)); 
        if (ch == L'\n') { 
            pending_.pop_back(); 
            emit(); 
        } 
    }
    return traits_type::not_eof(ch);
}

int PthOutputBuffer::sync() { 
    emit(); 
    return target_->pubsync(); 
}

void PthOutputBuffer::setTarget(std::wstreambuf* target) { 
    target_ = target; 
}

void PthOutputBuffer::start() { 
    if (format_ == OutputFormat::Json) write(L"[\n"); 
}

void PthOutputBuffer::finish() { 
    emit(); 
    if (format_ == OutputFormat::Json) write(L"\n]\n"); 
}

// PthPipeBuffer
PthPipeBuffer::PthPipeBuffer(FILE* file) : file_(file) { 
    setp(buffer_, buffer_ + 1024); 
}

std::wstreambuf::int_type PthPipeBuffer::overflow(int_type ch) { 
    if (ch != traits_type::eof()) { 
        *pptr() = static_cast<wchar_t>(ch); 
        pbump(1); 
    } 
    return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); 
}

int PthPipeBuffer::sync() { 
    auto count = pptr() - pbase(); 
    if (count && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), file_) != static_cast<size_t>(count)) return -1; 
    setp(buffer_, buffer_ + 1024); 
    return std::fflush(file_) == 0 ? 0 : -1; 
}

// PthOutputSession
PthOutputSession::PthOutputSession(OutputFormat format, const std::wstring& command) 
    : old_(std::wcout.rdbuf()), buffer_(old_, format) {
    if (!command.empty()) {
        pipe_ = _wpopen(command.c_str(), L"w");
        if (pipe_) {
            pipeBuffer_ = new PthPipeBuffer(pipe_);
            buffer_.setTarget(pipeBuffer_);
            std::wcout.rdbuf(&buffer_);
        }
    } else {
        std::wcout.rdbuf(&buffer_);
    }
    buffer_.start();
}

PthOutputSession::~PthOutputSession() { 
    std::wcout.flush(); 
    buffer_.finish(); 
    std::wcout.rdbuf(old_); 
    delete pipeBuffer_; 
    if (pipe_) _pclose(pipe_); 
}
