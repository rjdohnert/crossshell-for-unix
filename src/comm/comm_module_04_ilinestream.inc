    class ILineStream {
    public:
        virtual ~ILineStream() = default;
        virtual bool open() = 0;
        virtual bool get_next_line(std::string& out_line) = 0;
        virtual const std::string& get_name() const = 0;
        virtual uint64_t get_line_number() const = 0;
        virtual bool is_eof() const = 0;
    };

