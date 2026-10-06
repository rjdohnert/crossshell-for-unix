#pragma once

#include "threads_options.hpp"
#include "threads.hpp"

class ThreadCountApp {
public:
    explicit ThreadCountApp(AppConfig config);

    void run();

private:
    AppConfig config_;

    static std::wstring toLower(std::wstring s);
};
