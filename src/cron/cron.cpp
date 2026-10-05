/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * SINGLE FILE INDEX: cron.cpp
 * ============================================================================
 * WinCron - Object-Oriented Cron Task Scheduler & Daemon for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [CRON JOB & SCHEDULE SPECIFICATION] ... CronJob, CronTimeMatcher classes
 * 2. [OPTIONS & CONFIGURATION] ............. CronOptions class (CLI parsing & flags)
 * 3. [STRUCTURED OUTPUT REPORTER] .......... CronReporter class (JSON/CSV/Table/Pipe)
 * 4. [CRONTAB PARSER & SCHEDULER ENGINE] ... CrontabParser, CronEngine classes
 * 5. [APPLICATION CONTROLLER] .............. CronApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
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

// ============================================================================
// 1. CRON JOB & SCHEDULE SPECIFICATION
// ============================================================================

#include "cron_module_01_cronjob.inc"
#include "cron_module_02_crontimematcher.inc"
#include "cron_module_03_cronoptions.inc"
#include "cron_module_04_cronreporter.inc"
#include "cron_module_05_crontabparser.inc"
#include "cron_module_06_cronengine.inc"
#include "cron_module_07_cronapp.inc"
