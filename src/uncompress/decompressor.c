#include "decompressor.h"
#include "dictionary.h"

LzwDecompressor* LzwDecompressor_Create(void) {
    return (LzwDecompressor*)calloc(1, sizeof(LzwDecompressor));
}

void LzwDecompressor_Destroy(LzwDecompressor* self) {
    free(self);
}

DecompressResult LzwDecompressor_Process(LzwDecompressor* self, IStream* inStream, IStream* outStream) {
    uint8_t* inBuf = (uint8_t*)calloc(1, IO_BUFFER_SIZE + BUFFER_PADDING);
    uint8_t* outBuf = (uint8_t*)malloc(IO_BUFFER_SIZE);
    if (!inBuf || !outBuf) {
        free(inBuf);
        free(outBuf);
        return DECOMP_MEM_ERROR;
    }

    size_t inSize = 0;
    while (inSize < 3) {
        int64_t n = inStream->vptr->Read(inStream, inBuf + inSize, 3 - inSize);
        if (n <= 0) {
            free(inBuf);
            free(outBuf);
            return (inSize == 0) ? DECOMP_NOT_COMPRESSED : DECOMP_CORRUPT;
        }
        inSize += (size_t)n;
    }

    if (inBuf[0] != LZW_MAGIC_1 || inBuf[1] != LZW_MAGIC_2 || (inBuf[2] & LZW_RESERVED_MASK)) {
        free(inBuf);
        free(outBuf);
        return DECOMP_NOT_COMPRESSED;
    }

    uint8_t maxBits = inBuf[2] & LZW_BIT_MASK;
    bool blockMode = (inBuf[2] & LZW_BLOCK_MODE) != 0;
    if (maxBits < LZW_INIT_BITS || maxBits > LZW_MAX_BITS) {
        free(inBuf);
        free(outBuf);
        return DECOMP_CORRUPT;
    }

    LzwDictionary* dict = LzwDictionary_Create(maxBits);
    if (!dict) {
        free(inBuf);
        free(outBuf);
        return DECOMP_MEM_ERROR;
    }
    LzwDictionary_Reset(dict, blockMode);

    uint32_t nBits = LZW_INIT_BITS;
    uint32_t maxMaxCode = 1U << maxBits;
    uint32_t maxCode = (nBits == maxBits) ? maxMaxCode : ((1U << nBits) - 1U);
    uint32_t bitMask = (1U << nBits) - 1U;

    int32_t oldCode = -1;
    uint8_t finChar = 0;
    size_t outPos = 0;
    size_t posBits = 24; // 3 header bytes consumed
    int64_t rSize = 0;
    DecompressResult result = DECOMP_OK;

    self->bytesIn = 3;
    self->bytesOut = 0;

    do {
        if (g_Interrupted) {
            result = DECOMP_IO_ERROR;
            goto cleanup;
        }
resetbuf:;
        {
            size_t o = posBits >> 3;
            size_t e = (o <= inSize) ? (inSize - o) : 0;
            if (e > 0 && o > 0) {
                memmove(inBuf, inBuf + o, e);
            }
            inSize = e;
            posBits = 0;
        }

        if (inSize < IO_BUFFER_SIZE) {
            rSize = inStream->vptr->Read(inStream, inBuf + inSize, IO_BUFFER_SIZE - inSize);
            if (rSize < 0) {
                result = DECOMP_IO_ERROR;
                goto cleanup;
            }
            if (rSize > 0) {
                inSize += (size_t)rSize;
                self->bytesIn += (uint64_t)rSize;
            }
        }

        // Zero safety padding past buffer
        memset(inBuf + inSize, 0, BUFFER_PADDING);

        size_t inBits = (rSize > 0) ? ((inSize - (inSize % nBits)) << 3)
                                    : ((inSize >= 2) ? ((inSize << 3) - (nBits - 1)) : 0);

        while (inBits > posBits) {
            if (dict->freeEnt > maxCode) {
                posBits = ((posBits - 1) + ((nBits << 3) - ((posBits - 1 + (nBits << 3)) % (nBits << 3))));
                nBits++;
                maxCode = (nBits == maxBits) ? maxMaxCode : ((1U << nBits) - 1U);
                bitMask = (1U << nBits) - 1U;
                goto resetbuf;
            }

            // Extract code
            const uint8_t* p = &inBuf[posBits >> 3];
            uint32_t raw = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
            uint32_t code = (raw >> (posBits & 7)) & bitMask;
            posBits += nBits;

            if (oldCode == -1) {
                if (code >= 256) {
                    result = DECOMP_CORRUPT;
                    goto cleanup;
                }
                finChar = (uint8_t)code;
                oldCode = (int32_t)code;
                outBuf[outPos++] = finChar;
                self->bytesOut++;
                continue;
            }

            if (code == LZW_CLEAR_CODE && blockMode) {
                memset(dict->prefixTab, 0, 256 * sizeof(uint16_t));
                dict->freeEnt = LZW_FIRST_CODE - 1;
                posBits = ((posBits - 1) + ((nBits << 3) - ((posBits - 1 + (nBits << 3)) % (nBits << 3))));
                nBits = LZW_INIT_BITS;
                bitMask = (1U << nBits) - 1U;
                maxCode = (nBits == maxBits) ? maxMaxCode : ((1U << nBits) - 1U);
                goto resetbuf;
            }

            uint32_t inCode = code;
            uint8_t* stackTop = dict->stack + LZW_DICT_SIZE;
            uint8_t* stackPtr = stackTop;

            if (code >= dict->freeEnt) {
                if (code > dict->freeEnt) {
                    result = DECOMP_CORRUPT;
                    goto cleanup;
                }
                *--stackPtr = finChar;
                code = (uint32_t)oldCode;
            }

            // Walk prefix chain (Hardened with stack limit checking against CVE-2006-1168)
            while (code >= 256) {
                if (stackPtr <= dict->stack) {
                    result = DECOMP_CORRUPT;
                    goto cleanup;
                }
                *--stackPtr = dict->suffixTab[code];
                code = dict->prefixTab[code];
            }

            if (stackPtr <= dict->stack) {
                result = DECOMP_CORRUPT;
                goto cleanup;
            }
            finChar = dict->suffixTab[code];
            *--stackPtr = finChar;

            // Output stack characters
            size_t strLen = (size_t)(stackTop - stackPtr);
            if (outPos + strLen >= IO_BUFFER_SIZE) {
                do {
                    size_t chunk = IO_BUFFER_SIZE - outPos;
                    if (strLen < chunk) chunk = strLen;
                    if (chunk > 0) {
                        memcpy(outBuf + outPos, stackPtr, chunk);
                        outPos += chunk;
                        stackPtr += chunk;
                        strLen -= chunk;
                    }
                    if (outPos >= IO_BUFFER_SIZE) {
                        if (outStream->vptr->Write(outStream, outBuf, outPos) != (int64_t)outPos) {
                            result = DECOMP_IO_ERROR;
                            goto cleanup;
                        }
                        self->bytesOut += outPos;
                        outPos = 0;
                    }
                } while (strLen > 0);
            } else {
                memcpy(outBuf + outPos, stackPtr, strLen);
                outPos += strLen;
                self->bytesOut += strLen;
            }

            // Add code entry to dictionary
            if ((code = dict->freeEnt) < maxMaxCode) {
                dict->prefixTab[code] = (uint16_t)oldCode;
                dict->suffixTab[code] = finChar;
                dict->freeEnt = code + 1;
            }
            oldCode = (int32_t)inCode;
        }
    } while (rSize > 0);

    if (outPos > 0) {
        if (outStream->vptr->Write(outStream, outBuf, outPos) != (int64_t)outPos) {
            result = DECOMP_IO_ERROR;
            goto cleanup;
        }
    }

cleanup:
    LzwDictionary_Destroy(dict);
    free(inBuf);
    free(outBuf);
    return result;
}
