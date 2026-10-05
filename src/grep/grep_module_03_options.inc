// ============================================================================
// 3. GREP OPTIONS & CONFIGURATION
// ============================================================================
struct GrepOptions {
    std::vector<std::string> patterns;
    std::string key_path = "";
    std::string where_expr = "";
    std::string extract_path = "";

    bool ignore_case = false;
    bool invert_match = false;
    bool fixed_strings = false;
    bool quiet = false;
    bool no_messages = false;
    bool text_mode = false;
    bool byte_offset = false;
    bool null_output = false;
    bool binary_files_without_match = false;
    bool binary_mode = false;
    bool word_regexp = false;
    bool line_regexp = false;
    bool line_number = false;
    bool count_only = false;
    bool files_with_matches = false;
    bool files_without_match = false;
    bool only_matching = false;
    bool with_filename = false;
    bool no_filename = false;
    bool recursive = false;
    bool json_mode = false;
    bool text_only = false;
    bool color = true;
    bool color_explicit = false;

    std::string directory_action = "read";
    std::string devices_action = "read";

    int max_count = -1;
    int before_context = 0;
    int after_context = 0;

    std::vector<std::string> file_paths;
    std::vector<std::string> object_sources;
};

const std::string COLOR_RESET   = "\033[0m";
const std::string COLOR_MATCH   = "\033[1;31m";
const std::string COLOR_FILE    = "\033[35m";
const std::string COLOR_LINE    = "\033[32m";
const std::string COLOR_SEP     = "\033[36m";

