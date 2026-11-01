#ifndef CRON_H
#define CRON_H

// Shared umbrella header for the cron daemon: Win32 + standard library includes.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <set>

#endif // CRON_H
