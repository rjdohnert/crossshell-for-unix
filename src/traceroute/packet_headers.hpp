#pragma once

#include "traceroute.hpp"

#pragma pack(push, 1)
struct IPHeader {
    BYTE  ver_len;       // Version (4 bits) + Header length (4 bits)
    BYTE  tos;           // Type of service
    WORD  total_len;     // Total length
    WORD  id;            // Identification
    WORD  flags_offset;  // Flags + Fragment offset
    BYTE  ttl;           // Time to live
    BYTE  protocol;      // Protocol
    WORD  checksum;      // Checksum
    IN_ADDR src_addr;    // Source address
    IN_ADDR dst_addr;    // Destination address
};

struct ICMPHeader {
    BYTE type;           // ICMP Type
    BYTE code;           // ICMP Code
    WORD checksum;       // ICMP Checksum
    WORD id;             // ICMP ID
    WORD sequence;       // ICMP Sequence
};

struct UDPHeader {
    WORD src_port;       // Source port
    WORD dst_port;       // Destination port
    WORD length;         // UDP length
    WORD checksum;       // UDP checksum
};
#pragma pack(pop)
