#ifndef CPIO_SECURITY_H
#define CPIO_SECURITY_H

#include "cpio.h"

// Path-traversal defense and wildcard matching.
bool HasReservedDeviceComponent(const char* path);
bool SanitizePath(const char* rawPath, char** outCleanPath);
bool MatchWildcard(const char* pat, const char* str);
bool ShouldProcessFile(const char* filename, const CpioConfig* cfg);

#endif // CPIO_SECURITY_H
