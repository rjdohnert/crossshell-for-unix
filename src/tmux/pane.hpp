#pragma once

#include "rect.hpp"
#include "tmux.hpp"
#include "virtual_screen.hpp"

class Pane {
public:
    int id;
    std::string title;
    Rect bounds{};
    HPCON hPC{ INVALID_HANDLE_VALUE };
    HANDLE hPipeIn{ INVALID_HANDLE_VALUE };
    HANDLE hPipeOut{ INVALID_HANDLE_VALUE };
    HANDLE hProcess{ INVALID_HANDLE_VALUE };
    std::atomic<bool> alive{ true };
    std::thread readerThread;
    VirtualScreen screen;
    std::function<void()> onDirty;

    Pane(int id, const std::string& title, std::function<void()> dirtyCb);

    ~Pane();

    bool Start(const std::wstring& cmd, int w, int h);

    void Resize(int w, int h);

    void SendInput(const std::string& data);
};
