class BitWriter {
    std::vector<uint8_t>& out;
    uint32_t buffer = 0;
    int bits_in_buffer = 0;
public:
    BitWriter(std::vector<uint8_t>& output) : out(output) {}
    
    void write(uint32_t code, int num_bits) {
        buffer |= (code << bits_in_buffer);
        bits_in_buffer += num_bits;
        while (bits_in_buffer >= 8) {
            out.push_back(static_cast<uint8_t>(buffer & 0xFF));
            buffer >>= 8;
            bits_in_buffer -= 8;
        }
    }
    
    void flush() {
        if (bits_in_buffer > 0) {
            out.push_back(static_cast<uint8_t>(buffer & 0xFF));
            buffer = 0;
            bits_in_buffer = 0;
        }
    }
};

