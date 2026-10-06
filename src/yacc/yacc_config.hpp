#pragma once

#include "yacc.hpp"

struct Config {
    std::string input_file;
    std::string output_file;
    std::string header_file;
    std::string verbose_file;
    std::string sym_prefix = "yy";
    std::string file_prefix;
    bool yacc_mode = false;
    bool generate_header = false;
    bool verbose = false;
    bool debug_mode = false;
    bool no_lines = false;
    bool token_table = false;
    enum class OutputFormat { Human, Json, Csv, Table } output_format = OutputFormat::Human;
    std::string pipe_command;
};
