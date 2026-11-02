#include "cpio_security.h"
#include "cpio_path.h"

/* ========================================================================= */
/* SECURITY & PATH TRAVERSAL DEFENSE                                         */
/* ========================================================================= */

bool HasReservedDeviceComponent(const char* path) {
    const char* p = path;
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (!*p) break;

        char comp[64];
        size_t i = 0;
        while (*p && *p != '/' && *p != '\\') {
            if (i < sizeof(comp) - 1) comp[i++] = *p;
            p++;
        }
        comp[i] = '\0';

        char base[16];
        size_t j = 0;
        while (j < i && j < 15 && comp[j] != '.') {
            base[j] = (char)tolower((unsigned char)comp[j]);
            j++;
        }
        base[j] = '\0';

        if (strcmp(base, "con") == 0 || strcmp(base, "prn") == 0 ||
            strcmp(base, "aux") == 0 || strcmp(base, "nul") == 0 ||
            strcmp(base, "clock$") == 0 ||
            strcmp(base, "conin$") == 0 || strcmp(base, "conout$") == 0) return true;

        if ((strncmp(base, "com", 3) == 0 || strncmp(base, "lpt", 3) == 0) &&
            base[3] >= '1' && base[3] <= '9' && base[4] == '\0') return true;
    }
    return false;
}

bool SanitizePath(const char* rawPath, char** outCleanPath) {
    if (!rawPath || !outCleanPath) return false;

    const char* p = rawPath;

    if (isalpha((unsigned char)p[0]) && p[1] == ':') {
        p += 2;
    }

    while (*p == '/' || *p == '\\' || *p == '.') {
        if (*p == '.' && (p[1] == '/' || p[1] == '\\')) {
            p += 2;
        } else if (*p == '.' && p[1] == '\0') {
            p++;
        } else if (*p == '/' || *p == '\\') {
            p++;
        } else {
            break;
        }
    }

    if (*p == '\0') return false;

    char* temp = _strdup(p);
    if (!temp) return false;
    NormalizeSlashes(temp);

    if (strchr(temp, ':') != NULL) {
        fprintf(stderr, "cpio: security alert: illegal character ':' in '%s'\n", rawPath);
        free(temp);
        return false;
    }

    if (strcmp(temp, "..") == 0 ||
        strncmp(temp, "../", 3) == 0 ||
        strstr(temp, "/../") != NULL ||
        (strlen(temp) >= 3 && strcmp(temp + strlen(temp) - 3, "/..") == 0)) {
        fprintf(stderr, "cpio: security alert: directory traversal blocked: '%s'\n", rawPath);
        free(temp);
        return false;
    }

    if (HasReservedDeviceComponent(temp)) {
        fprintf(stderr, "cpio: security alert: reserved DOS device in '%s'\n", rawPath);
        free(temp);
        return false;
    }

    *outCleanPath = temp;
    return true;
}

bool MatchWildcard(const char* pat, const char* str) {
    while (*pat) {
        if (*pat == '*') {
            while (*pat == '*') pat++;
            if (!*pat) return true;
            while (*str) {
                if (MatchWildcard(pat, str)) return true;
                str++;
            }
            return false;
        } else if (*pat == '?' || tolower((unsigned char)*pat) == tolower((unsigned char)*str)) {
            if (!*str) return false;
            pat++;
            str++;
        } else {
            return false;
        }
    }
    return *str == '\0';
}

bool ShouldProcessFile(const char* filename, const CpioConfig* cfg) {
    if (cfg->patternCount == 0) return true;
    for (int i = 0; i < cfg->patternCount; ++i) {
        if (MatchWildcard(cfg->patterns[i], filename)) return true;
    }
    return false;
}
