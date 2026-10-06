#pragma once

#include "xorriso.hpp"

constexpr UINT32 ISO_SECTOR_SIZE = 2048;
constexpr UINT32 ISO_SYSTEM_AREA_SECTORS = 16;
constexpr UINT64 ISO_PVD_OFFSET = static_cast<UINT64>(ISO_SYSTEM_AREA_SECTORS) * ISO_SECTOR_SIZE;
constexpr UINT32 IO_BUFFER_SIZE = 1024 * 1024;

#pragma pack(push, 1)

struct BothEndian16 {
    uint16_t lsb;
    uint16_t msb;
    void set(uint16_t val) {
        lsb = val;
        msb = _byteswap_ushort(val);
    }
};

struct BothEndian32 {
    uint32_t lsb;
    uint32_t msb;
    void set(uint32_t val) {
        lsb = val;
        msb = _byteswap_ulong(val);
    }
};

struct IsoDateTimePVD {
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char minute[2];
    char second[2];
    char hundredths[2];
    int8_t gmt_offset;
};

struct IsoDateTimeDir {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    int8_t gmt_offset;
};

struct IsoPrimaryVolumeDescriptor {
    uint8_t type;
    char id[5];
    uint8_t version;
    uint8_t unused1;
    char system_id[32];
    char volume_id[32];
    uint8_t unused2[8];
    BothEndian32 volume_space_size;
    uint8_t unused3[32];
    BothEndian16 volume_set_size;
    BothEndian16 volume_sequence_number;
    BothEndian16 logical_block_size;
    BothEndian32 path_table_size;
    uint32_t type_l_path_table;
    uint32_t opt_type_l_path_table;
    uint32_t type_m_path_table;
    uint32_t opt_type_m_path_table;
    uint8_t root_directory_record[34];
    char volume_set_id[128];
    char publisher_id[128];
    char data_preparer_id[128];
    char application_id[128];
    char copyright_file_id[37];
    char abstract_file_id[37];
    char bibliographic_file_id[37];
    IsoDateTimePVD creation_date;
    IsoDateTimePVD modification_date;
    IsoDateTimePVD expiration_date;
    IsoDateTimePVD effective_date;
    uint8_t file_structure_version;
    uint8_t unused4;
    uint8_t application_data[512];
    uint8_t reserved[653];
};

struct IsoDirRecordHeader {
    uint8_t length;
    uint8_t ext_attr_length;
    BothEndian32 extent_lba;
    BothEndian32 data_length;
    IsoDateTimeDir date;
    uint8_t file_flags;
    uint8_t file_unit_size;
    uint8_t interleave_gap_size;
    BothEndian16 volume_sequence_number;
    uint8_t name_len;
};

#pragma pack(pop)
