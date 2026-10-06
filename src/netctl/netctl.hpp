/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * netctl v2.0 - High-Speed Asynchronous Network Packet Analyzer
 */

#ifndef NETCTL_HPP
#define NETCTL_HPP

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <cctype>
#include <cstring>
#include <limits>
#include <utility>

#ifdef NETCTL_ENABLE_NPCAP
#include <pcap.h>
#endif

// --- Protocol Header Definitions (1-Byte Alignment) ---
#pragma pack(push, 1)

struct IPv4Header {
    uint8_t  ver_ihl;         // Version (4 bits) + Internet Header Length (4 bits)
    uint8_t  tos;             // Type of Service
    uint16_t total_length;    // Total Length
    uint16_t id;              // Identification
    uint16_t flags_offset;    // Flags (3 bits) + Fragment Offset (13 bits)
    uint8_t  ttl;             // Time to Live
    uint8_t  protocol;        // Protocol (1=ICMP, 6=TCP, 17=UDP)
    uint16_t checksum;        // Header Checksum
    uint32_t src_ip;          // Source IP
    uint32_t dest_ip;         // Destination IP
};

struct IPv6Header {
    uint32_t v_tc_fl;         // Version (4b), Traffic Class (8b), Flow Label (20b)
    uint16_t payload_len;     // Payload Length
    uint8_t  next_header;     // Next Header (Protocol)
    uint8_t  hop_limit;       // Hop Limit (TTL)
    in6_addr src_ip;          // Source IPv6 Address
    in6_addr dest_ip;         // Destination IPv6 Address
};

struct TCPHeader {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset_reserved; // Data Offset (4 bits) + Reserved
    uint8_t  flags;                // TCP Flags
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
};

struct UDPHeader {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
};

struct ICMPHeader {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

struct DNSHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

struct PcapGlobalHeader {
    uint32_t magic_number  = 0xa1b2c3d4;
    uint16_t version_major = 2;
    uint16_t version_minor = 4;
    int32_t  thiszone      = 0;
    uint32_t sigfigs       = 0;
    uint32_t snaplen       = 65535;
    uint32_t network       = 101; // DLT_RAW (IP)
};

struct PcapPacketHeader {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};

#pragma pack(pop)

// --- Data Models ---
struct RawPacket {
    std::vector<uint8_t> data;
    std::chrono::system_clock::time_point timestamp;
    sockaddr_storage src_addr{};
    int family; // AF_INET or AF_INET6
};

struct Config {
    std::string interface_ip = "";
    int         address_family = AF_INET;
    std::string filter_protocol = "ALL";
    std::string output_file = "";
    std::string pcap_file = "";
    uint16_t    filter_port = 0;
    uint32_t    max_packets = 0;
    bool        verbose = false;
    bool        hex_dump = false;
    bool        no_color = false;
    bool        use_npcap = false;
};

// --- ANSI Color Formatting Helper ---
namespace Color {
    inline const char* RESET   = "\033[0m";
    inline const char* BOLD    = "\033[1m";
    inline const char* RED     = "\033[31m";
    inline const char* GREEN   = "\033[32m";
    inline const char* YELLOW  = "\033[33m";
    inline const char* BLUE    = "\033[34m";
    inline const char* MAGENTA = "\033[35m";
    inline const char* CYAN    = "\033[36m";
    inline const char* WHITE   = "\033[37m";
    inline const char* GRAY    = "\033[90m";
}

#endif // NETCTL_HPP
