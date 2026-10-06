#include "registry_line_formatter.hpp"
#include "registry_pipe_buffer.hpp"

void RegistryLineFormatter::emit() {
        if (pending_.empty()) return;
        std::string line = pending_;
        if (format_ == RegistryOutputFormat::Csv) {
            std::string escaped = "\"";
            for (char ch : line) escaped += ch == '"' ? "\"\"" : std::string(1, ch);
            line = escaped + "\"\n";
        } else if (format_ == RegistryOutputFormat::Table) {
            line += "\n";
        }
        target_->sputn(line.data(), static_cast<std::streamsize>(line.size()));
        pending_.clear();
    }

RegistryLineFormatter::RegistryLineFormatter(std::streambuf* target, RegistryOutputFormat format) : target_(target), format_(format) {}

RegistryLineFormatter::int_type RegistryLineFormatter::overflow(int_type ch)  { if (ch != traits_type::eof()) { if (ch == '\n') emit(); else pending_.push_back(static_cast<char>(ch)); } return traits_type::not_eof(ch); }

int RegistryLineFormatter::sync()  { emit(); return target_->pubsync(); }
