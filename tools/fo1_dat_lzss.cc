#include "fo1_dat_lzss.h"

#include <string.h>

namespace fallout {

static inline void fillDecodeBuffer(FILE* stream);
static inline void decodeChunkToBuf(unsigned int type, unsigned char** dest, unsigned int* length);

static unsigned char decodeBuffer[1024];
static unsigned char* decodeBufferPosition;
static unsigned int decodeBytesLeft;
static int ringBufferIndex;
static unsigned char* decodeBufferEnd;
static unsigned char ringBuffer[4116];

int fo1LzssDecodeToBuf(FILE* in, unsigned char* dest, unsigned int length)
{
    unsigned char* curr = dest;
    unsigned char byte;

    memset(ringBuffer, ' ', 4078);
    ringBufferIndex = 4078;
    decodeBufferEnd = decodeBuffer;
    decodeBufferPosition = decodeBuffer;
    decodeBytesLeft = length;

    while (length > 16) {
        fillDecodeBuffer(in);

        length -= 1;
        byte = *decodeBufferPosition++;
        decodeChunkToBuf(byte & 0x01, &curr, &length);
        decodeChunkToBuf(byte & 0x02, &curr, &length);
        decodeChunkToBuf(byte & 0x04, &curr, &length);
        decodeChunkToBuf(byte & 0x08, &curr, &length);
        decodeChunkToBuf(byte & 0x10, &curr, &length);
        decodeChunkToBuf(byte & 0x20, &curr, &length);
        decodeChunkToBuf(byte & 0x40, &curr, &length);
        decodeChunkToBuf(byte & 0x80, &curr, &length);
    }

    do {
        if (length == 0) {
            break;
        }

        fillDecodeBuffer(in);

        length -= 1;
        byte = *decodeBufferPosition++;

        if (length == 0) break;
        decodeChunkToBuf(byte & 0x01, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x02, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x04, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x08, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x10, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x20, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x40, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x80, &curr, &length);

        if (length == 0) break;

        length -= 1;
        byte = *decodeBufferPosition++;

        if (length == 0) break;
        decodeChunkToBuf(byte & 0x01, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x02, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x04, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x08, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x10, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x20, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x40, &curr, &length);
        if (length == 0) break;
        decodeChunkToBuf(byte & 0x80, &curr, &length);
    } while (0);

    return static_cast<int>(curr - dest);
}

static inline void fillDecodeBuffer(FILE* stream)
{
    size_t bytesToRead;
    size_t bytesRead;

    if (decodeBytesLeft != 0 && decodeBufferEnd - decodeBufferPosition <= 16) {
        if (decodeBufferPosition == decodeBufferEnd) {
            decodeBufferEnd = decodeBuffer;
        } else {
            memmove(decodeBuffer, decodeBufferPosition, decodeBufferEnd - decodeBufferPosition);
            decodeBufferEnd = decodeBuffer + (decodeBufferEnd - decodeBufferPosition);
        }

        decodeBufferPosition = decodeBuffer;

        bytesToRead = 1024 - (decodeBufferEnd - decodeBuffer);
        if (bytesToRead > decodeBytesLeft) {
            bytesToRead = decodeBytesLeft;
        }

        bytesRead = fread(decodeBufferEnd, 1, bytesToRead, stream);
        decodeBufferEnd += bytesRead;
        decodeBytesLeft -= bytesRead;
    }
}

static inline void decodeChunkToBuf(unsigned int type, unsigned char** dest, unsigned int* length)
{
    unsigned char low;
    unsigned char high;
    int dictOffset;
    int dictIndex;
    int chunkLength;
    int index;

    if (type != 0) {
        *length -= 1;
        *(*dest) = *decodeBufferPosition++;
        ringBuffer[ringBufferIndex] = *(*dest)++;
        ringBufferIndex += 1;
        ringBufferIndex &= 0xFFF;
    } else {
        *length -= 2;
        low = *decodeBufferPosition++;
        high = *decodeBufferPosition++;
        dictOffset = low | ((high & 0xF0) << 4);
        chunkLength = (high & 0x0F) + 3;

        for (index = 0; index < chunkLength; index++) {
            dictIndex = (dictOffset + index) & 0xFFF;
            *(*dest) = ringBuffer[dictIndex];
            ringBuffer[ringBufferIndex] = *(*dest)++;
            ringBufferIndex += 1;
            ringBufferIndex &= 0xFFF;
        }
    }
}

} // namespace fallout
