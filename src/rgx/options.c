#include "options.h"
#include "allocation.h"

RgxOptions* RgxOptions_New(void) {
    RgxOptions* opts = (RgxOptions*)rgx_calloc(1, sizeof(RgxOptions));
    DWORD console_mode;
    opts->show_line_numbers = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &console_mode) != 0;
    opts->filename_mode = -1;
    opts->color_mode = COLOR_AUTO;
    opts->max_depth = 0;
    opts->max_count = 0;
    opts->threads = 0;
    return opts;
}

void RgxOptions_Free(RgxOptions* self) {
    if (!self) return;
    for (size_t i = 0; i < self->pattern_count; i++) free(self->patterns[i]);
    free(self->patterns);
    for (size_t i = 0; i < self->path_count; i++) free(self->paths[i]);
    free(self->paths);
    for (size_t index = 0; index < self->glob_count; ++index) free(self->globs[index]);
    free(self->globs);
    for(size_t index=0;index<self->type_count;++index)free(self->types[index]);free(self->types);
    for(size_t index=0;index<self->type_not_count;++index)free(self->type_not[index]);free(self->type_not);
    for(size_t index=0;index<self->type_add_count;++index)free(self->type_add[index]);free(self->type_add);
    for(size_t index=0;index<self->type_clear_count;++index)free(self->type_clear[index]);free(self->type_clear);
    free(self->replacement);
    free(self);
}

/* ========================================================================= */
/*                   2. OOP MATCHER INTERFACE & IMPLEMENTATIONS              */
/* ========================================================================= */
