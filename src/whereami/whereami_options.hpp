#pragma once

#include "whereami.hpp"

enum class OutputFormat {
    Raw,          // Default: 41.386905,2.144257
    Json,         // Machine readable JSON object
    Sexagesimal,  // DMS: 41° 23' 12.86" N, 2° 8' 39.32" E
    Human,        // Formatted human-readable multi-line report
    Address       // Civic street/city address only
};

struct CliOptions {
    OutputFormat format = OutputFormat::Raw;
    bool show_accuracy = false;
    bool show_altitude = false;
    bool show_address = false;
    bool show_network = false;
    bool verbose = false;
    bool quiet = false;
    int timeout_seconds = 5;
    std::string from_provider = "auto"; // "auto", "sensor", "network"
    std::string basedir = "";
    std::string statedir = "";
    bool show_help = false;
    bool show_version = false;
};

// Minimal self-contained JSON extractor
