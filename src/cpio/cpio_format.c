#include "cpio_format.h"

/* ========================================================================= */
/* FORMAT CONVERSION & DECODING (HEX, OCTAL, BINARY)                         */
/* ========================================================================= */

uint32_t ParseHex8(const char* p) {
    uint32_t val = 0;
    for (int i = 0; i < 8; i++) {
        char c = p[i];
        uint32_t nibble;
        if (c >= '0' && c <= '9')      nibble = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') nibble = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') nibble = (uint32_t)(c - 'A' + 10);
        else return 0;
        val = (val << 4) | nibble;
    }
    return val;
}

uint32_t ParseOctal(const char* p, int len) {
    uint32_t val = 0;
    for (int i = 0; i < len; i++) {
        if (p[i] < '0' || p[i] > '7') break;
        val = (val << 3) | (p[i] - '0');
    }
    return val;
}

size_t CalcPadding4(size_t bytes) {
    return (4 - (bytes % 4)) % 4;
}

size_t CalcPadding2(size_t bytes) {
    return (2 - (bytes % 2)) % 2;
}

uint32_t SynthesizePosixMode(const struct _stat64* st) {
    uint32_t mode = 0;
    if (S_ISDIR(st->st_mode)) {
        mode = 0040755;
    } else {
        mode = 0100644;
        if (st->st_mode & _S_IEXEC) mode |= 0000111;
        if (!(st->st_mode & _S_IWRITE)) mode &= ~0000222;
    }
    return mode;
}

/* ========================================================================= */
/* MULTI-FORMAT HEADER DISPATCHER                                            */
/* ========================================================================= */

/**
 * Universal header reader: Auto-detects and decodes SVR4 newc (070701),
 * SVR4 CRC (070702), POSIX.1 odc (070707), and Old Binary (0x71c7 / 0xc771).
 */
bool ReadCpioHeader(FILE* in, GenericCpioEntry* e) {
    memset(e, 0, sizeof(*e));

    unsigned char magic[6];
    size_t r = fread(magic, 1, 2, in);
    if (r == 0) return false;
    if (r < 2) {
        fprintf(stderr, "cpio: fatal: premature EOF on magic bytes\n");
        SetFatal();
        return false;
    }

    /* Check Binary cpio format */
    uint16_t bMagic = (uint16_t)(magic[0] | (magic[1] << 8));
    if (bMagic == 0x71c7 || bMagic == 0xc771) {
        e->format = (bMagic == 0x71c7) ? FMT_BIN_LE : FMT_BIN_BE;
        unsigned char raw[24];
        if (fread(raw, 1, 24, in) != 24) return false;

        #define GET_U16(idx) (e->format == FMT_BIN_LE ? \
            (uint16_t)(raw[idx] | (raw[idx+1] << 8)) : \
            (uint16_t)((raw[idx] << 8) | raw[idx+1]))

        #define GET_U32(idx) (e->format == FMT_BIN_LE ? \
            (uint32_t)(raw[idx] | (raw[idx+1] << 8) | (raw[idx+2] << 16) | (raw[idx+3] << 24)) : \
            (uint32_t)((raw[idx] << 24) | (raw[idx+1] << 16) | (raw[idx+2] << 8) | raw[idx+3]))

        e->devMaj = 0;
        e->devMin = GET_U16(0);
        e->ino    = GET_U16(2);
        e->mode   = GET_U16(4);
        e->uid    = GET_U16(6);
        e->gid    = GET_U16(8);
        e->nlink  = GET_U16(10);
        /* raw[12..13] is rdev */
        e->mtime    = GET_U32(14);
        e->nameSize = GET_U16(18);
        e->fileSize = GET_U32(20);
        e->namePad  = CalcPadding2(e->nameSize);
        e->dataPad  = CalcPadding2((size_t)e->fileSize);
        return true;
    }

    /* Read remaining 4 bytes of ASCII magic */
    if (fread(magic + 2, 1, 4, in) != 4) return false;

    if (memcmp(magic, "070701", 6) == 0 || memcmp(magic, "070702", 6) == 0) {
        e->format = (magic[5] == '1') ? FMT_NEWC : FMT_CRC;
        char hdr[CPIO_HEADER_SIZE - 6];
        if (fread(hdr, 1, CPIO_HEADER_SIZE - 6, in) != CPIO_HEADER_SIZE - 6) return false;

        e->ino      = ParseHex8(hdr);
        e->mode     = ParseHex8(hdr + 8);
        e->uid      = ParseHex8(hdr + 16);
        e->gid      = ParseHex8(hdr + 24);
        e->nlink    = ParseHex8(hdr + 32);
        e->mtime    = ParseHex8(hdr + 40);
        e->fileSize = ParseHex8(hdr + 48);
        e->devMaj   = ParseHex8(hdr + 56);
        e->devMin   = ParseHex8(hdr + 64);
        e->nameSize = ParseHex8(hdr + 88);
        e->check    = ParseHex8(hdr + 96);
        e->namePad  = CalcPadding4(CPIO_HEADER_SIZE + e->nameSize);
        e->dataPad  = CalcPadding4((size_t)e->fileSize);
        return true;
    }

    if (memcmp(magic, "070707", 6) == 0) {
        e->format = FMT_ODC;
        char hdr[70]; /* 76 - 6 */
        if (fread(hdr, 1, 70, in) != 70) return false;

        e->devMin   = ParseOctal(hdr, 6);
        e->ino      = ParseOctal(hdr + 6, 6);
        e->mode     = ParseOctal(hdr + 12, 6);
        e->uid      = ParseOctal(hdr + 18, 6);
        e->gid      = ParseOctal(hdr + 24, 6);
        e->nlink    = ParseOctal(hdr + 30, 6);
        /* hdr + 36 is rdev (6 bytes) */
        e->mtime    = ParseOctal(hdr + 42, 11);
        e->nameSize = ParseOctal(hdr + 53, 6);
        e->fileSize = ParseOctal(hdr + 59, 11);
        e->namePad  = 0; /* ODC format does not use padding */
        e->dataPad  = 0;
        return true;
    }

    fprintf(stderr, "cpio: fatal: unrecognized archive format signature '%.6s'\n", magic);
    SetFatal();
    return false;
}
