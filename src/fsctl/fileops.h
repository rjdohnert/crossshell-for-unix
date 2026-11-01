#ifndef FSCTL_FILEOPS_H
#define FSCTL_FILEOPS_H

#include "app.h"

void clipboard_copy_or_cut(bool is_cut);
void clipboard_paste(void);
void show_properties(void);
void rename_file(void);
void delete_item(void);

#endif // FSCTL_FILEOPS_H
