#pragma once

#include "archive_entry.hpp"
#include "unzip_options.hpp"
#include "unzip.hpp"

int listArchive(const UnzipOptions& opts, const std::vector<ArchiveEntry>& entries);
