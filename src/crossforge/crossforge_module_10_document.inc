class Document {
public:
    using LinePtr = std::shared_ptr<const std::string>;

    // Line storage wrapper that mimics the subset of std::vector<std::string>
    // used by the editor while storing lines as immutable shared pointers.
