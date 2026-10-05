// ============================================================================
// 2. EMBEDDED JSON ENGINE
// ============================================================================
struct Value;
using Object = std::map<std::string, Value>;
using Array  = std::vector<Value>;

struct Value {
    enum Type { NIL, BOOL, NUMBER, STRING, ARRAY, OBJECT } type = NIL;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    Value() : type(NIL), data(nullptr) {}
    Value(bool b) : type(BOOL), data(b) {}
    Value(double d) : type(NUMBER), data(d) {}
    Value(const std::string& s) : type(STRING), data(s) {}
    Value(const char* s) : type(STRING), data(std::string(s)) {}
    Value(Array a) : type(ARRAY), data(std::move(a)) {}
    Value(Object o) : type(OBJECT), data(std::move(o)) {}

    std::string to_string() const {
        if (type == NIL) return "";
        if (type == BOOL) return std::get<bool>(data) ? "true" : "false";
        if (type == NUMBER) {
            double d = std::get<double>(data);
            if (d == static_cast<long long>(d)) return std::to_string(static_cast<long long>(d));
            std::ostringstream ss;
            ss << std::setprecision(10) << d;
            return ss.str();
        }
        if (type == STRING) return std::get<std::string>(data);
        if (type == ARRAY) {
            std::string out = "[";
            const auto& arr = std::get<Array>(data);
            for (size_t i = 0; i < arr.size(); ++i) {
                out += arr[i].to_string() + (i + 1 < arr.size() ? ", " : "");
            }
            return out + "]";
        }
        if (type == OBJECT) {
            std::string out = "{";
            const auto& obj = std::get<Object>(data);
            size_t i = 0;
            for (const auto& [k, v] : obj) {
                out += "\"" + k + "\": " + v.to_string() + (++i < obj.size() ? ", " : "");
            }
            return out + "}";
        }
        return "";
    }

    double to_number() const {
        if (type == NUMBER) return std::get<double>(data);
        if (type == BOOL) return std::get<bool>(data) ? 1.0 : 0.0;
        if (type == STRING) {
            try { return std::stod(std::get<std::string>(data)); } catch (...) { return 0.0; }
        }
        return 0.0;
    }

    bool to_bool() const {
        if (type == NIL) return false;
        if (type == BOOL) return std::get<bool>(data);
        if (type == NUMBER) return std::get<double>(data) != 0.0;
        if (type == STRING) return !std::get<std::string>(data).empty() && std::get<std::string>(data) != "0";
        if (type == ARRAY) return !std::get<Array>(data).empty();
        if (type == OBJECT) return !std::get<Object>(data).empty();
        return true;
    }

    Value get_path(const std::string& key) const {
        if (type == OBJECT) {
            const auto& obj = std::get<Object>(data);
            auto it = obj.find(key);
            if (it != obj.end()) return it->second;
        } else if (type == ARRAY) {
            try {
                size_t idx = std::stoul(key);
                const auto& arr = std::get<Array>(data);
                if (idx < arr.size()) return arr[idx];
            } catch (...) {}
        }
        return Value();
    }
};

class MiniJsonParser {
    std::string src;
    size_t pos = 0;

    void skip_ws() {
        while (pos < src.size() && (std::isspace(static_cast<unsigned char>(src[pos])))) pos++;
    }
    char peek() { skip_ws(); return pos < src.size() ? src[pos] : '\0'; }
    char get()  { skip_ws(); return pos < src.size() ? src[pos++] : '\0'; }

    std::string parse_string() {
        get();
        std::string s;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < src.size()) {
                char esc = src[pos++];
                if (esc == 'n') s += '\n';
                else if (esc == 't') s += '\t';
                else if (esc == 'r') s += '\r';
                else s += esc;
            } else {
                s += c;
            }
        }
        return s;
    }

    Value parse_number() {
        size_t start = pos;
        if (src[pos] == '-') pos++;
        while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
            pos++;
        }
        try {
            return Value(std::stod(src.substr(start, pos - start)));
        } catch (...) {
            return Value(0.0);
        }
    }

public:
    MiniJsonParser(std::string s) : src(std::move(s)) {}

    Value parse_value() {
        char c = peek();
        if (c == '{') {
            get();
            Object obj;
            while (peek() != '}' && peek() != '\0') {
                if (peek() == ',') { get(); continue; }
                std::string k = parse_string();
                if (peek() == ':') get();
                obj[k] = parse_value();
            }
            if (peek() == '}') get();
            return Value(obj);
        } else if (c == '[') {
            get();
            Array arr;
            while (peek() != ']' && peek() != '\0') {
                if (peek() == ',') { get(); continue; }
                arr.push_back(parse_value());
            }
            if (peek() == ']') get();
            return Value(arr);
        } else if (c == '"') {
            return Value(parse_string());
        } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
            return parse_number();
        } else if (c == 't' || c == 'f') {
            std::string b;
            while (std::isalpha(static_cast<unsigned char>(peek()))) b += get();
            return Value(b == "true");
        } else if (c == 'n') {
            for (int i = 0; i < 4; ++i) get();
            return Value();
        }
        return Value();
    }
};

