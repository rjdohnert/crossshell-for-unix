#include "win32_handle.hpp"

Win32Handle::Win32Handle(HANDLE h) : h_(h) {}

Win32Handle::~Win32Handle() { if (isValid()) CloseHandle(h_); }

bool Win32Handle::isValid() const { return h_ != INVALID_HANDLE_VALUE && h_ != nullptr; }

Win32Handle::operator HANDLE() const { return h_; }
