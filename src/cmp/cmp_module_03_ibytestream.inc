    class IByteStream {
    public:
        virtual ~IByteStream() = default;
        virtual bool open() = 0;
        virtual bool read_byte(uint8_t& byte) = 0;
        virtual void skip(uint64_t count) = 0;
        virtual bool is_eof() const = 0;
        virtual const std::string& get_name() const = 0;
        virtual bool has_error() const = 0;
    };

