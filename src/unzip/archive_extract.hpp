#pragma once

#include "archive_entry.hpp"
#include "unzip_options.hpp"
#include "unzip.hpp"

int extractArchive(const UnzipOptions& opts, const fs::path& zipPath, const std::vector<ArchiveEntry>& entries);
