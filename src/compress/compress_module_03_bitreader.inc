class BitReader {
    const std::vector<uint8_t>& in;
    size_t byte_idx = 3; // LZW data starts after the 3-byte header
    uint32_t buffer = 0;
    int bits_in_buffer = 0;
public:
    BitReader(const std::vector<uint8_t>& input) : in(input) {}
    
    bool read(uint32_t& code, int num_bits) {
        while (bits_in_buffer < num_bits) {
            if (byte_idx >= in.size()) {
                if (bits_in_buffer == 0) return false;
                break; 
            }
            buffer |= (static_cast<uint32_t>(in[byte_idx]) << bits_in_buffer);
            byte_idx++;
            bits_in_buffer += 8;
        }
        if (bits_in_buffer < num_bits) {
            return false;
        }
        code = buffer & ((1ULL << num_bits) - 1);
        buffer >>= num_bits;
        bits_in_buffer -= num_bits;
        return true;
    }
};

// ==========================================
// LZW Core Logic
// ==========================================

