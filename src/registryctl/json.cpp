#include "json.hpp"

namespace Json {

Data::Data() {}

Data::Data(std::nullptr_t) {}

Value::Value(std::nullptr_t) : type(Type::Null), data(nullptr) {}

Value::Value(bool b) : type(Type::Boolean), data(b) {}

Value::Value(double d) : type(Type::Number), data(d) {}

Value::Value(int i) : type(Type::Number), data(static_cast<double>(i)) {}

Value::Value(const std::string& s) : type(Type::String), data(s) {}

Value::Value(const char* s) : type(Type::String), data(std::string(s)) {}

Value::Value(const Array& a) : type(Type::Array), data(a) {}

Value::Value(const Object& o) : type(Type::Object), data(o) {}

bool Value::IsNull() const { return type == Type::Null; }

bool Value::IsBool() const { return type == Type::Boolean; }

bool Value::IsNumber() const { return type == Type::Number; }

bool Value::IsString() const { return type == Type::String; }

bool Value::IsArray() const { return type == Type::Array; }

bool Value::IsObject() const { return type == Type::Object; }

bool Value::AsBool(bool def ) const { return IsBool() ? data.get<bool>() : def; }

double Value::AsNumber(double def ) const { return IsNumber() ? data.get<double>() : def; }

int Value::AsInt(int def ) const { return IsNumber() ? static_cast<int>(data.get<double>()) : def; }

std::string Value::AsString(const std::string& def ) const { return IsString() ? data.get<std::string>() : def; }

const Array& Value::AsArray() const { static const Array empty; return IsArray() ? data.get<Array>() : empty; }

const Object& Value::AsObject() const { static const Object empty; return IsObject() ? data.get<Object>() : empty; }

bool Value::HasKey(const std::string& key) const {
            if (!IsObject()) return false;
            const auto& obj = data.get<Object>();
            return obj.find(key) != obj.end();
        }

Value Value::operator[](const std::string& key) const {
            if (IsObject()) {
                const auto& obj = data.get<Object>();
                auto it = obj.find(key);
                if (it != obj.end()) return it->second;
            }
            return Value();
        }

void Parser::SkipWhitespace() {
            while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) {
                pos++;
            }
        }

char Parser::Peek() { SkipWhitespace(); return pos < src.size() ? src[pos] : '\0'; }

char Parser::Get() { SkipWhitespace(); return pos < src.size() ? src[pos++] : '\0'; }

std::string Parser::ParseString() {
            Get(); // Consume opening quote
            std::string res;
            while (pos < src.size()) {
                char c = src[pos++];
                if (c == '"') return res;
                if (c == '\\' && pos < src.size()) {
                    char esc = src[pos++];
                    switch (esc) {
                        case '"':  res += '"'; break;
                        case '\\': res += '\\'; break;
                        case '/':  res += '/'; break;
                        case 'b':  res += '\b'; break;
                        case 'f':  res += '\f'; break;
                        case 'n':  res += '\n'; break;
                        case 'r':  res += '\r'; break;
                        case 't':  res += '\t'; break;
                        case 'u': {
                            if (pos + 4 <= src.size()) {
                                std::string hex = src.substr(pos, 4);
                                pos += 4;
                                uint32_t code = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                                if (code <= 0x7F) {
                                    res += static_cast<char>(code);
                                } else if (code <= 0x7FF) {
                                    res += static_cast<char>(0xC0 | ((code >> 6) & 0x1F));
                                    res += static_cast<char>(0x80 | (code & 0x3F));
                                } else {
                                    res += static_cast<char>(0xE0 | ((code >> 12) & 0x0F));
                                    res += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                    res += static_cast<char>(0x80 | (code & 0x3F));
                                }
                            }
                            break;
                        }
                        default: res += esc; break;
                    }
                } else {
                    res += c;
                }
            }
            return res;
        }

Value Parser::ParseNumber() {
            size_t start = pos;
            if (src[pos] == '-') pos++;
            while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
                pos++;
            }
            try {
                double val = std::stod(src.substr(start, pos - start));
                return Value(val);
            } catch (...) {
                return Value(0.0);
            }
        }

Parser::Parser(const std::string& input) : src(input), pos(0) {}

Value Parser::ParseValue() {
            SkipWhitespace();
            char c = Peek();
            if (c == '{') return ParseObject();
            if (c == '[') return ParseArray();
            if (c == '"') return Value(ParseString());
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') return ParseNumber();
            if (src.rfind("true", pos) == pos) { pos += 4; return Value(true); }
            if (src.rfind("false", pos) == pos) { pos += 5; return Value(false); }
            if (src.rfind("null", pos) == pos) { pos += 4; return Value(nullptr); }
            return Value();
        }

Value Parser::ParseObject() {
            Get(); // Consume '{'
            Object obj;
            while (Peek() != '}' && Peek() != '\0') {
                if (Peek() != '"') break;
                std::string key = ParseString();
                SkipWhitespace();
                if (Get() != ':') break;
                obj[key] = ParseValue();
                if (Peek() == ',') Get();
            }
            if (Peek() == '}') Get();
            return Value(obj);
        }

Value Parser::ParseArray() {
            Get(); // Consume '['
            Array arr;
            while (Peek() != ']' && Peek() != '\0') {
                arr.push_back(ParseValue());
                if (Peek() == ',') Get();
            }
            if (Peek() == ']') Get();
            return Value(arr);
        }

std::string Stringify(const Value& val, int indent ) {
        std::string ind(indent * 2, ' ');
        switch (val.type) {
            case Type::Null: return "null";
            case Type::Boolean: return val.AsBool() ? "true" : "false";
            case Type::Number: {
                std::ostringstream ss;
                ss << val.AsNumber();
                return ss.str();
            }
            case Type::String: {
                std::ostringstream ss;
                ss << "\"";
                for (char c : val.AsString()) {
                    if (c == '"') ss << "\\\"";
                    else if (c == '\\') ss << "\\\\";
                    else if (c == '\n') ss << "\\n";
                    else if (c == '\r') ss << "\\r";
                    else if (c == '\t') ss << "\\t";
                    else ss << c;
                }
                ss << "\"";
                return ss.str();
            }
            case Type::Array: {
                const auto& arr = val.AsArray();
                if (arr.empty()) return "[]";
                std::string res = "[\n";
                for (size_t i = 0; i < arr.size(); ++i) {
                    res += ind + "  " + Stringify(arr[i], indent + 1);
                    if (i + 1 < arr.size()) res += ",";
                    res += "\n";
                }
                res += ind + "]";
                return res;
            }
            case Type::Object: {
                const auto& obj = val.AsObject();
                if (obj.empty()) return "{}";
                std::string res = "{\n";
                size_t i = 0;
                for (const auto& entry : obj) {
                    const auto& k = entry.first;
                    const auto& v = entry.second;
                    res += ind + "  \"" + k + "\": " + Stringify(v, indent + 1);
                    if (++i < obj.size()) res += ",";
                    res += "\n";
                }
                res += ind + "}";
                return res;
            }
        }
        return "null";
    }
}
