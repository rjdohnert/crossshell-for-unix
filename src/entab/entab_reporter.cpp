/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "entab_reporter.hpp"

#include <ostream>
#include <string_view>
#include <utility>

namespace {

bool IsContinuation(unsigned char value) {
    return (value & 0xc0) == 0x80;
}

std::size_t Utf8Length(unsigned char first) {
    if (first >= 0xc2 && first <= 0xdf) return 2;
    if (first >= 0xe0 && first <= 0xef) return 3;
    if (first >= 0xf0 && first <= 0xf4) return 4;
    return 0;
}

bool IsCsvWhitespace(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool IsFormulaPrefix(unsigned char value) {
    return value == '=' || value == '+' || value == '-' || value == '@';
}

std::FILE* CreateTemporaryFile() {
#ifdef _WIN32
    std::FILE* file = nullptr;
    return tmpfile_s(&file) == 0 ? file : nullptr;
#else
    return std::tmpfile();
#endif
}

} // namespace

EntabReporter::EntabReporter(EntabOutputFormat format, std::ostream& output)
    : format_(format), output_(output) {}

EntabReporter::~EntabReporter() {
    if (csvWhitespaceSpill_) std::fclose(csvWhitespaceSpill_);
}

bool EntabReporter::Begin() {
    if (began_) return false;
    began_ = true;
    switch (format_) {
        case EntabOutputFormat::Human:
            break;
        case EntabOutputFormat::Json:
            output_ << "{\"output\":\"";
            break;
        case EntabOutputFormat::Csv:
            output_ << "Output\n\"";
            break;
        case EntabOutputFormat::Table:
            output_ << "Output\n------\n";
            break;
    }
    return static_cast<bool>(output_);
}

bool EntabReporter::WriteByte(unsigned char value) {
    if (!began_ || finished_) return false;
    if (format_ == EntabOutputFormat::Human) {
        output_.put(static_cast<char>(value));
        return static_cast<bool>(output_);
    }
    if (format_ == EntabOutputFormat::Csv) return WriteCsvByte(value);
    return WriteUtf8Byte(value);
}

bool EntabReporter::WriteCsvByte(unsigned char value) {
    if (!csvPrefixDecided_) {
        if (IsCsvWhitespace(value)) {
            if (csvWhitespaceSpill_) {
                if (std::fwrite(&value, 1, 1, csvWhitespaceSpill_) != 1) {
                    return false;
                }
            } else if (csvLeadingWhitespace_.size() < 4096) {
                csvLeadingWhitespace_.push_back(static_cast<char>(value));
            } else {
                csvWhitespaceSpill_ = CreateTemporaryFile();
                if (!csvWhitespaceSpill_ ||
                    std::fwrite(csvLeadingWhitespace_.data(), 1,
                                csvLeadingWhitespace_.size(),
                                csvWhitespaceSpill_) !=
                        csvLeadingWhitespace_.size() ||
                    std::fwrite(&value, 1, 1, csvWhitespaceSpill_) != 1) {
                    return false;
                }
                csvLeadingWhitespace_.clear();
            }
            return true;
        } else {
            if (IsFormulaPrefix(value)) output_.put('\'');
        }
        if (!FlushCsvLeadingWhitespace()) return false;
        csvPrefixDecided_ = true;
        if (!output_) {
            return false;
        }
    }
    return WriteUtf8Byte(value);
}

bool EntabReporter::FlushCsvLeadingWhitespace() {
    output_.write(csvLeadingWhitespace_.data(),
                  static_cast<std::streamsize>(csvLeadingWhitespace_.size()));
    csvLeadingWhitespace_.clear();
    if (!output_) return false;
    if (!csvWhitespaceSpill_) return true;
    if (std::fflush(csvWhitespaceSpill_) != 0 ||
        std::fseek(csvWhitespaceSpill_, 0, SEEK_SET) != 0) {
        return false;
    }

    char buffer[4096];
    for (;;) {
        const std::size_t count =
            std::fread(buffer, 1, sizeof(buffer), csvWhitespaceSpill_);
        if (count > 0) {
            output_.write(buffer, static_cast<std::streamsize>(count));
            if (!output_) return false;
        }
        if (count < sizeof(buffer)) {
            return !std::ferror(csvWhitespaceSpill_);
        }
    }
}

bool EntabReporter::WriteUtf8Byte(unsigned char value) {
    if (utf8Pending_.empty()) {
        if (value < 0x80) return EmitEncodedByte(value);
        utf8Expected_ = Utf8Length(value);
        if (utf8Expected_ == 0) return WriteControlEscape(value);
        utf8Pending_.push_back(static_cast<char>(value));
        return true;
    }

    const unsigned char first =
        static_cast<unsigned char>(utf8Pending_.front());
    const std::size_t nextIndex = utf8Pending_.size();
    bool valid = IsContinuation(value);
    if (nextIndex == 1) {
        valid = valid && (first != 0xe0 || value >= 0xa0) &&
                (first != 0xed || value <= 0x9f) &&
                (first != 0xf0 || value >= 0x90) &&
                (first != 0xf4 || value <= 0x8f);
    }
    if (!valid) {
        if (!EmitInvalidUtf8()) return false;
        return WriteUtf8Byte(value);
    }

    utf8Pending_.push_back(static_cast<char>(value));
    if (utf8Pending_.size() == utf8Expected_) {
        const std::string validSequence = std::move(utf8Pending_);
        utf8Pending_.clear();
        utf8Expected_ = 0;
        return WriteRaw(validSequence);
    }
    return true;
}

bool EntabReporter::EmitInvalidUtf8() {
    for (unsigned char value : utf8Pending_) {
        if (!WriteControlEscape(value)) return false;
    }
    utf8Pending_.clear();
    utf8Expected_ = 0;
    return true;
}

bool EntabReporter::WriteEncodedByte(unsigned char value) {
    if (format_ == EntabOutputFormat::Json) {
        switch (value) {
            case '\\': output_ << "\\\\"; break;
            case '"': output_ << "\\\""; break;
            case '\b': output_ << "\\b"; break;
            case '\f': output_ << "\\f"; break;
            case '\n': output_ << "\\n"; break;
            case '\r': output_ << "\\r"; break;
            case '\t': output_ << "\\t"; break;
            default:
                if (value < 0x20) {
                    if (!WriteControlEscape(value)) return false;
                } else {
                    output_.put(static_cast<char>(value));
                }
        }
    } else if (format_ == EntabOutputFormat::Csv) {
        if (value == '"') output_ << "\"\"";
        else if ((value < 0x20 && value != '\n' && value != '\r' &&
                  value != '\t') || value == 0x7f) {
            if (!WriteControlEscape(value)) return false;
        } else {
            output_.put(static_cast<char>(value));
        }
    } else if (format_ == EntabOutputFormat::Table) {
        if (value == '\t') output_ << "\\t";
        else if (value == '\n') output_ << "\\n";
        else if (value == '\r') output_ << "\\r";
        else if (value < 0x20 || value == 0x7f) {
            if (!WriteControlEscape(value)) return false;
        } else {
            output_.put(static_cast<char>(value));
        }
    }
    return static_cast<bool>(output_);
}

bool EntabReporter::EmitEncodedByte(unsigned char value) {
    return WriteEncodedByte(value);
}

bool EntabReporter::WriteControlEscape(unsigned char value) {
    static constexpr char hex[] = "0123456789abcdef";
    if (format_ == EntabOutputFormat::Json) {
        output_ << "\\u00" << hex[value >> 4] << hex[value & 0x0f];
    } else {
        output_ << "\\x" << hex[value >> 4] << hex[value & 0x0f];
    }
    return static_cast<bool>(output_);
}

bool EntabReporter::WriteRaw(std::string_view value) {
    output_.write(value.data(), static_cast<std::streamsize>(value.size()));
    return static_cast<bool>(output_);
}

bool EntabReporter::Finish() {
    if (!began_ || finished_) return false;
    if (format_ == EntabOutputFormat::Csv && !csvPrefixDecided_) {
        if (!FlushCsvLeadingWhitespace()) return false;
        csvPrefixDecided_ = true;
    }
    if (!EmitInvalidUtf8()) return false;
    switch (format_) {
        case EntabOutputFormat::Human:
            break;
        case EntabOutputFormat::Json:
            output_ << "\"}\n";
            break;
        case EntabOutputFormat::Csv:
            output_ << "\"\n";
            break;
        case EntabOutputFormat::Table:
            output_.put('\n');
            break;
    }
    finished_ = true;
    return static_cast<bool>(output_);
}
