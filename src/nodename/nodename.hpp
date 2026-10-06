/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * nodename - print or set system hostname
 */

#ifndef NODENAME_HPP
#define NODENAME_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>

enum class NameQueryMode {
    ShortName,
    DomainOnly,
    FullyQualified
};

#endif // NODENAME_HPP
