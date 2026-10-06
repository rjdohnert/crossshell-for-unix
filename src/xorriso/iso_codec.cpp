#include "iso_codec.hpp"

void IsoCodec::PadCopy(char* dest, const std::string& src, size_t maxLen) {
        size_t copyLen = (std::min)(src.length(), maxLen);
        std::memcpy(dest, src.data(), copyLen);
        if (copyLen < maxLen) {
            std::memset(dest + copyLen, ' ', maxLen - copyLen);
        }
    }
