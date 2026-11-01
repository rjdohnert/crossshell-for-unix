#ifndef FSCTL_FORMAT_H
#define FSCTL_FORMAT_H

#include "app.h"

void format_size(unsigned long long bytes, wchar_t *out, size_t out_sz);
void format_filetime(FILETIME ft, wchar_t *out, size_t out_sz);

#endif // FSCTL_FORMAT_H
