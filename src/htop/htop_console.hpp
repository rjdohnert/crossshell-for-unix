#pragma once

#include "htop_types.hpp"

class ConsoleRenderer {
private:
    HANDLE m_hOut;
    HANDLE m_hIn;
    DWORD m_origOutMode = 0;
    DWORD m_origInMode = 0;
    SHORT m_width = 80;
    SHORT m_height = 25;
    bool m_isConsoleInput = false;
    bool m_outModeChanged = false;
    bool m_inModeChanged = false;
    bool m_stdoutModeChanged = false;
    bool m_altScreenActive = false;
    int m_origStdoutMode = -1;
    std::wstring m_buffer;

public:
    ConsoleRenderer();
    ~ConsoleRenderer();

    ConsoleRenderer(const ConsoleRenderer&) = delete;
    ConsoleRenderer& operator=(const ConsoleRenderer&) = delete;

    bool Initialize();
    void UpdateDimensions();
    SHORT GetWidth() const;
    SHORT GetHeight() const;
    bool IsConsoleInput() const;
    void ClearBuffer();
    void Write(const std::wstring& text);
    bool Flush();
    std::wstring DrawBar(int width, double percent, const wchar_t* color);
};
