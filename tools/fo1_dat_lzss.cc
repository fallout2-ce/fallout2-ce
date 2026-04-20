// NOTE: Functions in these module are somewhat different from what can be seen
// in IDA because of two new helper functions that deal with incoming bits. I
// bet something like these were implemented via function-like macro in the
// same manner zlib deals with bits. The pattern is so common in this module so
// I made an exception and extracted it into separate functions to increase
// readability.

#include "fo1_dat_lzss.h"

#include <string.h>

namespace fallout {

static inline bool lzss_fill_decode_buffer(FILE* stream);
static inline bool lzss_read_decode_byte(FILE* stream, unsigned char* value);
static inline bool lzss_decode_chunk_to_buf(unsigned int type, FILE* stream, unsigned char** dest, unsigned int* length);

constexpr size_t lzssDecodeBufferSize = 1024;
constexpr unsigned int lzssRingBufferSize = 4116;
constexpr unsigned int lzssRingBufferMask = 0xFFF;
constexpr unsigned int lzssDecodeTriggerWindow = 16;
constexpr unsigned int lzssMatchLengthBias = 3;
constexpr unsigned char lzssInitialRingFill = ' ';
constexpr int lzssInitialRingIndex = 4078;

// 0x6B0860
static unsigned char decode_buffer[lzssDecodeBufferSize];

// 0x6B0C60
static unsigned char* decode_buffer_position;

// 0x6B0C64
static unsigned int decode_bytes_left;

// 0x6B0C68
static int ring_buffer_index;

// 0x6B0C6C
static unsigned char* decode_buffer_end;

// 0x6B0C70
static unsigned char ring_buffer[lzssRingBufferSize];

// 0x4CA260
int lzss_decode_to_buf(FILE* in, unsigned char* dest, unsigned int length)
{
    unsigned char* curr;
    unsigned char byte;

    curr = dest;
    memset(ring_buffer, lzssInitialRingFill, lzssInitialRingIndex);
    ring_buffer_index = lzssInitialRingIndex;
    decode_buffer_end = decode_buffer;
    decode_buffer_position = decode_buffer;
    decode_bytes_left = length;

    while (length > 16) {
        if (!lzss_read_decode_byte(in, &byte)) {
            return -1;
        }

        length -= 1;
        if (!lzss_decode_chunk_to_buf(byte & 0x01, in, &curr, &length)) return -1;
        if (!lzss_decode_chunk_to_buf(byte & 0x02, in, &curr, &length)) return -1;
        if (!lzss_decode_chunk_to_buf(byte & 0x04, in, &curr, &length)) return -1;
        if (!lzss_decode_chunk_to_buf(byte & 0x08, in, &curr, &length)) return -1;
        if (!lzss_decode_chunk_to_buf(byte & 0x10, in, &curr, &length)) return -1;
        if (!lzss_decode_chunk_to_buf(byte & 0x20, in, &curr, &length)) return -1;
        if (!lzss_decode_chunk_to_buf(byte & 0x40, in, &curr, &length)) return -1;
        if (!lzss_decode_chunk_to_buf(byte & 0x80, in, &curr, &length)) return -1;
    }

    do {
        if (length == 0) break;

        if (!lzss_read_decode_byte(in, &byte)) {
            return -1;
        }

        length -= 1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x01, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x02, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x04, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x08, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x10, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x20, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x40, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x80, in, &curr, &length)) return -1;

        if (length == 0) break;

        if (!lzss_read_decode_byte(in, &byte)) {
            return -1;
        }

        length -= 1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x01, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x02, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x04, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x08, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x10, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x20, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x40, in, &curr, &length)) return -1;

        if (length == 0) break;
        if (!lzss_decode_chunk_to_buf(byte & 0x80, in, &curr, &length)) return -1;
    } while (0);

    return curr - dest;
}

static inline bool lzss_fill_decode_buffer(FILE* stream)
{
    size_t bytes_to_read;
    size_t bytes_read;

    if (decode_bytes_left != 0 && decode_buffer_end - decode_buffer_position <= lzssDecodeTriggerWindow) {
        if (decode_buffer_position == decode_buffer_end) {
            decode_buffer_end = decode_buffer;
        } else {
            memmove(decode_buffer, decode_buffer_position, decode_buffer_end - decode_buffer_position);
            decode_buffer_end = decode_buffer + (decode_buffer_end - decode_buffer_position);
        }

        decode_buffer_position = decode_buffer;

        bytes_to_read = lzssDecodeBufferSize - (decode_buffer_end - decode_buffer);
        if (bytes_to_read > decode_bytes_left) {
            bytes_to_read = decode_bytes_left;
        }

        if (bytes_to_read == 0) {
            return false;
        }

        bytes_read = fread(decode_buffer_end, 1, bytes_to_read, stream);
        if (bytes_read != bytes_to_read) {
            return false;
        }

        decode_buffer_end += bytes_read;
        decode_bytes_left -= bytes_read;
    }

    return true;
}

static inline bool lzss_read_decode_byte(FILE* stream, unsigned char* value)
{
    if (!lzss_fill_decode_buffer(stream)) {
        return false;
    }

    if (decode_buffer_position >= decode_buffer_end) {
        return false;
    }

    *value = *decode_buffer_position++;
    return true;
}

static inline bool lzss_decode_chunk_to_buf(unsigned int type, FILE* stream, unsigned char** dest, unsigned int* length)
{
    unsigned char low;
    unsigned char high;
    int dict_offset;
    int dict_index;
    int chunk_length;
    int index;

    if (type != 0) {
        if (*length < 1) {
            return false;
        }

        unsigned char byte;
        if (!lzss_read_decode_byte(stream, &byte)) {
            return false;
        }

        *length -= 1;
        *(*dest) = byte;
        ring_buffer[ring_buffer_index] = *(*dest)++;
        ring_buffer_index += 1;
        ring_buffer_index &= lzssRingBufferMask;
    } else {
        if (*length < 2) {
            return false;
        }

        unsigned char bytes[2];
        if (!lzss_read_decode_byte(stream, &bytes[0]) || !lzss_read_decode_byte(stream, &bytes[1])) {
            return false;
        }

        *length -= 2;
        low = bytes[0];
        high = bytes[1];
        dict_offset = low | ((high & 0xF0) << 4);
        chunk_length = (high & 0x0F) + lzssMatchLengthBias;

        for (index = 0; index < chunk_length; index++) {
            dict_index = (dict_offset + index) & lzssRingBufferMask;
            *(*dest) = ring_buffer[dict_index];
            ring_buffer[ring_buffer_index] = *(*dest)++;
            ring_buffer_index += 1;
            ring_buffer_index &= lzssRingBufferMask;
        }
    }

    return true;
}

} // namespace fallout
