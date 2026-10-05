struct DecodeNode {
    uint16_t parent = 0;
    uint8_t character = 0;
};

void get_string(uint32_t code, const std::vector<DecodeNode>& dict, std::vector<uint8_t>& out_str) {
    if (code < 256) {
        out_str.push_back(static_cast<uint8_t>(code));
        return;
    }
    size_t start_idx = out_str.size();
    uint32_t curr = code;
    size_t depth = 0;
    while (curr >= 256) {
        out_str.push_back(dict[curr].character);
        curr = dict[curr].parent;
        depth++;
        if (depth > 65536) {
            throw std::runtime_error("Corrupted file: LZW loop detected");
        }
    }
    out_str.push_back(static_cast<uint8_t>(curr));
    std::reverse(out_str.begin() + start_idx, out_str.end());
}

std::vector<uint8_t> compress_lzw(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    if (input.empty()) return output;

    // Unix .Z Header: 0x1F, 0x9D, 0x90 (16 max bits, block mode)
    output.push_back(0x1F);
    output.push_back(0x9D);
    output.push_back(0x90);

    BitWriter bit_writer(output);

