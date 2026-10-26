#ifndef RGX_FILE_TYPES_H
#define RGX_FILE_TYPES_H
#include "options.h"
bool file_types_validate(const RgxOptions* options);
bool file_type_selected(const RgxOptions* options,const char* path);
void file_types_list(const RgxOptions* options);
#endif