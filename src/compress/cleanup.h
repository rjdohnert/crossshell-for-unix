#ifndef CLEANUP_H
#define CLEANUP_H

#include "common.h"

void cleanup_init(void);
void cleanup_shutdown(void);
void register_active_temp_file(const wchar_t *path);

#endif
