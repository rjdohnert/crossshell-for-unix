#pragma once

#include "registryctl.hpp"

namespace Json {
    enum class Type { Null, Boolean, Number, String, Array, Object };

    struct Value;
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;

    class Data {
        struct HolderBase {
            virtual ~HolderBase() {}
        };

        template <typename T>
        struct Holder : HolderBase {
            T value;
            explicit Holder(const T& v) : value(v) {}
        };

        std::shared_ptr<HolderBase> value;

    public:
        Data();
        Data(std::nullptr_t);

        template <typename T>
        Data(const T& v) : value(std::make_shared<Holder<T> >(v)) {}

        template <typename T>
        T& get() {
            return static_cast<Holder<T>*>(value.get())->value;
        }

        template <typename T>
        const T& get() const {
            return static_cast<const Holder<T>*>(value.get())->value;
        }
    };

    struct Value {
        Type type = Type::Null;
        Data data = nullptr;

        Value() = default;
        Value(std::nullptr_t);
        Value(bool b);
        Value(double d);
        Value(int i);
        Value(const std::string& s);
        Value(const char* s);
        Value(const Array& a);
        Value(const Object& o);

        bool IsNull() const;
        bool IsBool() const;
        bool IsNumber() const;
        bool IsString() const;
        bool IsArray() const;
        bool IsObject() const;

        bool AsBool(bool def = false) const;
        double AsNumber(double def = 0.0) const;
        int AsInt(int def = 0) const;
        std::string AsString(const std::string& def = "") const;
        const Array& AsArray() const;
        const Object& AsObject() const;

        bool HasKey(const std::string& key) const;

        Value operator[](const std::string& key) const;
    };

    class Parser {
    private:
        std::string src;
        size_t pos = 0;

        void SkipWhitespace();

        char Peek();
        char Get();

        std::string ParseString();

        Value ParseNumber();

    public:
        explicit Parser(const std::string& input);

        Value ParseValue();

        Value ParseObject();

        Value ParseArray();
    };

    std::string Stringify(const Value& val, int indent = 0);
}
