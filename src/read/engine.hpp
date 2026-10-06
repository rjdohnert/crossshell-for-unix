#pragma once

#include "read.hpp"
#include "options.hpp"
#include "reporter.hpp"

class ConsoleModeGuard {
private:
    HANDLE hIn;
    DWORD origMode{0};
    bool active{false};

public:
    explicit ConsoleModeGuard(HANDLE h);
    ~ConsoleModeGuard();
    void restore();
};

class ConsoleReader {
public:
    static int read(const ReadOptions& opts, std::wstring& result);
};

class PipeReader {
public:
    static int read(const ReadOptions& opts, std::wstring& result);
};

class ReadEngine {
private:
    ReadOptions options;

public:
    explicit ReadEngine(ReadOptions opts);
    int execute();
};
