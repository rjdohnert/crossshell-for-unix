#include "bison_structured_buffer.hpp"
#include "yacc_config.hpp"

void BisonStructuredBuffer::write(const std::string& text) { target_->sputn(text.data(), static_cast<std::streamsize>(text.size())); }

void BisonStructuredBuffer::emit() {
        if (pending_.empty()) return;
        if (format_ == Config::OutputFormat::Json) {
            if (!first_) write(",\n");
            first_ = false;
            std::string escaped = "{\"output\":\"";
            for (char ch : pending_) { if (ch == '"' || ch == '\\') escaped += '\\'; if (ch == '\r') escaped += "\\r"; else escaped += ch; }
            write(escaped + "\"}");
        } else if (format_ == Config::OutputFormat::Csv) {
            std::string escaped = "\"";
            for (char ch : pending_) escaped += ch == '"' ? "\"\"" : std::string(1, ch);
            write(escaped + "\"\n");
        } else {
            write(pending_ + "\n");
        }
        pending_.clear();
    }

BisonStructuredBuffer::BisonStructuredBuffer(std::streambuf* target, Config::OutputFormat format) : target_(target), format_(format) {
        if (format_ == Config::OutputFormat::Json) write("[\n");
        else if (format_ == Config::OutputFormat::Table) write("OUTPUT\n------\n");
    }

BisonStructuredBuffer::~BisonStructuredBuffer()  { emit(); if (format_ == Config::OutputFormat::Json) write("\n]\n"); }

BisonStructuredBuffer::int_type BisonStructuredBuffer::overflow(int_type ch)  { if (ch != traits_type::eof()) { if (ch == '\n') emit(); else pending_.push_back(static_cast<char>(ch)); } return traits_type::not_eof(ch); }

int BisonStructuredBuffer::sync()  { emit(); return target_->pubsync(); }
