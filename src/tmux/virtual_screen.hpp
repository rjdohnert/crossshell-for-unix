#pragma once

#include "cell.hpp"
#include "color.hpp"
#include "tmux.hpp"

class VirtualScreen {
public:
    int width = 80, height = 24;
    int cursorX = 0, cursorY = 0;
    int savedX = 0, savedY = 0;
    bool cursorVisible = true;

    Color curFg = Color::DefaultFg();
    Color curBg = Color::DefaultBg();
    bool curBold = false, curDim = false, curItalic = false, curUnderline = false, curInverse = false;

    std::vector<std::vector<Cell>> primaryGrid;
    std::vector<std::vector<Cell>> altGrid;
    bool usingAltBuffer = false;

    // Mouse Tracking Modes for Child TUIs
    bool mouseTracking = false;
    bool mouseSgrMode = false;
    bool mouseButtonMotion = false;
    bool mouseAllMotion = false;

    int scrollTop = 0, scrollBottom = 23;
    std::deque<std::vector<Cell>> scrollback;
    size_t maxScrollback = 3000;
    int scrollOffset = 0;

    enum State { Ground, Escape, Csi, Osc, Utf8_2, Utf8_3, Utf8_4 } parseState = Ground;
    std::vector<int> csiParams;
    int currentParam = 0;
    bool hasParam = false;
    bool csiPrivate = false;
    std::string oscString;
    uint32_t utf8Code = 0;
    int utf8BytesLeft = 0;

    std::function<void(const std::string&)> sendResponseCallback;
    std::mutex mtx;

    VirtualScreen(int w, int h);

    std::vector<std::vector<Cell>>& ActiveGrid();

    void ResizeGrid(int w, int h);

    void ScrollRegionUp(int top, int bottom);

    void ScrollRegionDown(int top, int bottom);

    void PutCodePoint(uint32_t cp);

    void ParseSgr();

    void ExecuteCsi(char finalChar);

    void ProcessBytes(const char* data, size_t len);
};
