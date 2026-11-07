#ifndef ENTAB_REPORTER_HPP
#define ENTAB_REPORTER_HPP

#include "entab_models.hpp"

#include <cstddef>
#include <cstdio>
#include <iosfwd>
#include <string>
#include <string_view>

class EntabReporter {
public:
    EntabReporter(EntabOutputFormat format, std::ostream& output);
    ~EntabReporter();
    EntabReporter(const EntabReporter&) = delete;
    EntabReporter& operator=(const EntabReporter&) = delete;
    bool Begin();
    bool WriteByte(unsigned char value);
    bool Finish();

private:
    bool WriteEncodedByte(unsigned char value);
    bool EmitEncodedByte(unsigned char value);
    bool EmitInvalidUtf8();
    bool WriteUtf8Byte(unsigned char value);
    bool WriteCsvByte(unsigned char value);
    bool FlushCsvLeadingWhitespace();
    bool WriteRaw(std::string_view value);
    bool WriteControlEscape(unsigned char value);

    EntabOutputFormat format_;
    std::ostream& output_;
    std::string utf8Pending_;
    std::string csvLeadingWhitespace_;
    std::FILE* csvWhitespaceSpill_ = nullptr;
    std::size_t utf8Expected_ = 0;
    bool csvPrefixDecided_ = false;
    bool began_ = false;
    bool finished_ = false;
};

#endif
