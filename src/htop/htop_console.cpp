#include "htop_console.hpp"

#include "htop_color.hpp"

#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <limits>

ConsoleRenderer::ConsoleRenderer()
    : m_hOut(INVALID_HANDLE_VALUE), m_hIn(INVALID_HANDLE_VALUE) {}

ConsoleRenderer::~ConsoleRenderer() {
    if (m_altScreenActive) {
        std::wcout << Color::RESET << L"\033[?1000l\033[?1006l\033[?25h\033[?1049l";
        std::wcout.flush();
    }
    if (m_stdoutModeChanged) _setmode(_fileno(stdout), m_origStdoutMode);
    if (m_inModeChanged) SetConsoleMode(m_hIn, m_origInMode);
    if (m_outModeChanged) SetConsoleMode(m_hOut, m_origOutMode);
}

bool ConsoleRenderer::Initialize() {
    m_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    m_hIn = GetStdHandle(STD_INPUT_HANDLE);

    if (m_hOut == INVALID_HANDLE_VALUE || m_hOut == NULL ||
        m_hIn == INVALID_HANDLE_VALUE || m_hIn == NULL) return false;
    if (!GetConsoleMode(m_hOut, &m_origOutMode)) return false;
    if (!GetConsoleMode(m_hIn, &m_origInMode)) return false;
    m_isConsoleInput = true;

    const DWORD newOutMode = m_origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    const DWORD newInMode =
        (m_origInMode | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS) &
        ~ENABLE_QUICK_EDIT_MODE;

    if (!SetConsoleMode(m_hOut, newOutMode)) return false;
    m_outModeChanged = true;
    if (!SetConsoleMode(m_hIn, newInMode)) return false;
    m_inModeChanged = true;

    m_origStdoutMode = _setmode(_fileno(stdout), _O_U16TEXT);
    if (m_origStdoutMode == -1) return false;
    m_stdoutModeChanged = true;

    std::wcout << L"\033[?1049h\033[?25l\033[H\033[?1000h\033[?1006h";
    std::wcout.flush();
    m_altScreenActive = true;
    UpdateDimensions();
    return true;
}

void ConsoleRenderer::UpdateDimensions() {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(m_hOut, &info)) {
        m_width = info.srWindow.Right - info.srWindow.Left + 1;
        m_height = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
}

SHORT ConsoleRenderer::GetWidth() const {
    return m_width;
}

SHORT ConsoleRenderer::GetHeight() const {
    return m_height;
}

bool ConsoleRenderer::IsConsoleInput() const {
    return m_isConsoleInput;
}

void ConsoleRenderer::ClearBuffer() {
    m_buffer.clear();
    const size_t targetChars =
        static_cast<size_t>((std::max<SHORT>)(m_width, 80)) *
        static_cast<size_t>((std::max<SHORT>)(m_height, 25)) * 2;
    if (m_buffer.capacity() < targetChars) m_buffer.reserve(targetChars);
    m_buffer += L"\033[2J\033[H";
}

void ConsoleRenderer::Write(const std::wstring& text) {
    m_buffer += text;
}

bool ConsoleRenderer::Flush() {
    if (m_buffer.empty()) return true;
    if (m_buffer.size() > (std::numeric_limits<DWORD>::max)()) return false;
    size_t offset = 0;
    while (offset < m_buffer.length()) {
        DWORD written = 0;
        const DWORD remaining = static_cast<DWORD>(m_buffer.length() - offset);
        if (!WriteConsoleW(m_hOut, m_buffer.c_str() + offset, remaining, &written, NULL) ||
            written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

std::wstring ConsoleRenderer::DrawBar(int width, double percent, const wchar_t* color) {
    int fill = static_cast<int>((percent / 100.0) * width);
    fill = (std::max)(0, (std::min)(width, fill));

    std::wstring bar;
    bar.reserve(static_cast<size_t>(width) + 16);
    bar += L"[";
    bar += color;
    bar.append(static_cast<size_t>(fill), L'|');
    bar += Color::RESET;
    bar.append(static_cast<size_t>(width - fill), L' ');
    bar += L"]";
    return bar;
}
