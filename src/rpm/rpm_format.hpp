#pragma once

#include "rpm.hpp"

constexpr uint32_t RPM_MAGIC_LEAD   = 0xedabeedb;
constexpr uint32_t RPM_HEADER_MAGIC = 0x8eade801;

enum RpmTagType : uint32_t {
    RPM_NULL_TYPE       = 0,
    RPM_CHAR_TYPE       = 1,
    RPM_INT8_TYPE       = 2,
    RPM_INT16_TYPE      = 3,
    RPM_INT32_TYPE      = 4,
    RPM_INT64_TYPE      = 5,
    RPM_STRING_TYPE     = 6,
    RPM_BIN_TYPE        = 7,
    RPM_STRING_ARRAY    = 8,
    RPM_I18NSTRING_TYPE = 9
};

enum RpmTag : uint32_t {
    RPMTAG_NAME              = 1000,
    RPMTAG_VERSION           = 1001,
    RPMTAG_RELEASE           = 1002,
    RPMTAG_EPOCH             = 1003,
    RPMTAG_SUMMARY           = 1004,
    RPMTAG_DESCRIPTION       = 1005,
    RPMTAG_BUILDTIME         = 1006,
    RPMTAG_SIZE              = 1009,
    RPMTAG_DISTRIBUTION      = 1010,
    RPMTAG_VENDOR            = 1011,
    RPMTAG_LICENSE           = 1014,
    RPMTAG_PACKAGER          = 1015,
    RPMTAG_GROUP             = 1016,
    RPMTAG_URL               = 1020,
    RPMTAG_OS                = 1021,
    RPMTAG_ARCH              = 1022,
    RPMTAG_PREIN             = 1023,
    RPMTAG_POSTIN            = 1024,
    RPMTAG_PREUN             = 1025,
    RPMTAG_POSTUN            = 1026,
    RPMTAG_OLDFILENAMES      = 1027,
    RPMTAG_FILESIZES         = 1028,
    RPMTAG_FILEMODES         = 1030,
    RPMTAG_FILEDIGESTS       = 1035,
    RPMTAG_FILEFLAGS         = 1037,
    RPMTAG_PROVIDENAME       = 1047,
    RPMTAG_REQUIREFLAGS      = 1048,
    RPMTAG_REQUIRENAME       = 1049,
    RPMTAG_REQUIREVERSION    = 1050,
    RPMTAG_CONFLICTNAME      = 1054,
    RPMTAG_PROVIDEFLAGS      = 1112,
    RPMTAG_PROVIDEVERSION    = 1113,
    RPMTAG_DIRINDEXES        = 1116,
    RPMTAG_BASENAMES         = 1117,
    RPMTAG_DIRNAMES          = 1118,
    RPMTAG_PAYLOADFORMAT     = 1124,
    RPMTAG_PAYLOADCOMPRESSOR = 1125,
    RPMTAG_PAYLOADFLAGS      = 1126,
    RPMTAG_SHA256HEADER      = 273
};

constexpr uint32_t RPMSENSE_LESS    = 0x02;
constexpr uint32_t RPMSENSE_GREATER = 0x04;
constexpr uint32_t RPMSENSE_EQUAL   = 0x08;

#pragma pack(push, 1)
struct RpmLeadRaw {
    uint32_t magic;
    uint8_t  major;
    uint8_t  minor;
    uint16_t type;
    uint16_t archnum;
    char     name[66];
    uint16_t osnum;
    uint16_t signature_type;
    char     reserved[16];
};

struct RpmIndexEntryRaw {
    uint32_t tag;
    uint32_t type;
    uint32_t offset;
    uint32_t count;
};
#pragma pack(pop)

inline uint16_t swap16(uint16_t val) { return (val << 8) | (val >> 8); }
inline uint32_t swap32(uint32_t val) {
    return ((val >> 24) & 0xff) | ((val << 8) & 0xff0000) |
           ((val >> 8) & 0xff00) | ((val << 24) & 0xff000000);
}
inline uint64_t swap64(uint64_t val) {
    return ((val & 0xFF00000000000000ULL) >> 56) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x000000FF00000000ULL) >> 8)  |
           ((val & 0x00000000FF000000ULL) << 8)  |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x00000000000000FFULL) << 56);
}
