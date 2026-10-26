#include "command_line_escaping.h"
#include "yum_context.h"

void AppendEscapedArgument(wchar_t* buffer, size_t bufferSize, const wchar_t* arg) {
    if (buffer[0] != L'\0') {
        wcsncat_s(buffer, bufferSize, L" ", _TRUNCATE);
    }

    bool needsQuotes = (wcslen(arg) == 0) || (wcschr(arg, L' ') != NULL) || (wcschr(arg, L'\t') != NULL);

    if (!needsQuotes) {
        wcsncat_s(buffer, bufferSize, arg, _TRUNCATE);
        return;
    }

    wcsncat_s(buffer, bufferSize, L"\"", _TRUNCATE);

    for (const wchar_t* p = arg; *p != L'\0'; ++p) {
        unsigned int numBackslashes = 0;
        while (*p == L'\\') {
            numBackslashes++;
            p++;
        }

        if (*p == L'\0') {
            for (unsigned int i = 0; i < numBackslashes * 2; ++i) {
                wcsncat_s(buffer, bufferSize, L"\\", _TRUNCATE);
            }
            break;
        } else if (*p == L'\"') {
            for (unsigned int i = 0; i < numBackslashes * 2 + 1; ++i) {
                wcsncat_s(buffer, bufferSize, L"\\", _TRUNCATE);
            }
            wcsncat_s(buffer, bufferSize, L"\"", _TRUNCATE);
        } else {
            for (unsigned int i = 0; i < numBackslashes; ++i) {
                wcsncat_s(buffer, bufferSize, L"\\", _TRUNCATE);
            }
            wchar_t ch[2] = { *p, L'\0' };
            wcsncat_s(buffer, bufferSize, ch, _TRUNCATE);
        }
    }

    wcsncat_s(buffer, bufferSize, L"\"", _TRUNCATE);
}

