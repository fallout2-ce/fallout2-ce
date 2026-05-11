#include "graph_lib.h"

#include <algorithm>
#include <cstring>

#include "color.h"
#include "db.h"
#include "debug.h"
#include "memory.h"
#include "palette.h"

namespace fallout {

static void _InitTree();
static void _InsertNode(int a1);
static void _DeleteNode(int a1);

// 0x596D90
static unsigned char _GreyTable[256];

// 0x596E90
static int* _dad_2;

// 0x596E94
static int _match_length;

// 0x596E98
static int _textsize;

// 0x596E9C
static int* _rson;

// 0x596EA0
static int* _lson;

// 0x596EA4
static unsigned char* _text_buf;

// 0x596EA8
static int _codesize;

// 0x596EAC
static int _match_position;

// 0x44EBC0
unsigned char HighRGB(unsigned char color)
{
    int rgb = Color2RGB(color);
    int r = (rgb & 0x7C00) >> 10;
    int g = (rgb & 0x3E0) >> 5;
    int b = (rgb & 0x1F);

    return std::max(std::max(r, g), b);
}

// 0x44ED98
// Loads LBM image file into a dstBuffer.
// Returns image width on success, -1 on error.
int load_lbm_to_buf(const char* path, unsigned char* dstBuffer, int xMin, int yMin, int xMax, int yMax)
{
    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        debugPrint("load_lbm_to_buf(%s): fileOpen failed\n", path);
        return -1;
    }

    unsigned char form[4];
    unsigned int formSize;
    unsigned char ilbm[4];
    if (fileRead(form, 1, 4, stream) != 4
        || memcmp(form, "FORM", 4) != 0
        || fileReadUInt32(stream, &formSize) == -1
        || fileRead(ilbm, 1, 4, stream) != 4) {
        debugPrint("load_lbm_to_buf(%s): incorrect LBM header\n", path);
        fileClose(stream);
        return -1;
    }

    int imgWidth = 0, imgHeight = 0;

    // NOTE: The original ASM's sliding-window scanner consumes the BMHD
    // chunk size as a 4-byte skip, reads only the 2-byte width, and then
    // lets the scanner drift through the remaining BMHD fields (height,
    // nPlanes, masking, comp, etc.) while searching for the next tag.
    // Height and compression are therefore never stored by the original.
    // The BODY decoder always runs as PackBits unconditionally.

    // paletteRemap[i] = system-palette index that best matches LBM colour i.
    // Built from CMAP via colorTable.  Index 0 is populated but never used
    // (pixel value 0 is always output as 0 — the transparent colour).
    unsigned char paletteRemap[256] = {};

    while (true) {
        unsigned char chunkType[4];
        unsigned int chunkSize;
        if (fileRead(chunkType, 1, 4, stream) != 4
            || fileReadUInt32(stream, &chunkSize) == -1) {
            break;
        }

        unsigned int aligned = (chunkSize + 1) & ~1u;

        if (memcmp(chunkType, "BMHD", 4) == 0) {
            // Original reads only w (2 bytes); h is read here to advance the
            // file position but its value is intentionally discarded.
            unsigned short w, h;
            if (fileReadUInt16(stream, &w) == -1
                || fileReadUInt16(stream, &h) == -1) break;
            imgWidth = w;
            imgHeight = h;

            // Skip every remaining BMHD field — the original never reads them.
            unsigned int bmhdRemaining = aligned - 4; // already consumed w + h
            if (bmhdRemaining > 0) fileSeek(stream, bmhdRemaining, SEEK_CUR);

        } else if (memcmp(chunkType, "CMAP", 4) == 0) {
            // The game DOES use its own palette, but it reconciles the two
            // by mapping each LBM color through colorTable (RGB555 → nearest
            // system index), storing the result in paletteRemap[].
            int entries = static_cast<int>(std::min(chunkSize / 3u, 256u));
            for (int i = 0; i < entries; i++) {
                unsigned char r, g, b;
                if (fileReadUInt8(stream, &r) == -1
                    || fileReadUInt8(stream, &g) == -1
                    || fileReadUInt8(stream, &b) == -1) {
                    fileClose(stream);
                    return -1;
                }
                int rgb555 = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
                paletteRemap[i] = _colorTable[rgb555];
            }
            unsigned int cmapRemaining = aligned - static_cast<unsigned int>(entries * 3);
            if (cmapRemaining > 0) fileSeek(stream, cmapRemaining, SEEK_CUR);

        } else if (memcmp(chunkType, "BODY", 4) == 0) {
            // Decompression, palette remapping, and clipped output in a single pass with no intermediate pixel buffer.
            // Output rules:
            //   pixel == 0  ->  write 0 unconditionally (transparent)
            //   pixel != 0  ->  write paletteRemap[pixel]
            //
            // The run-length case pre-remaps the pixel value once before the repeat loop.

            unsigned char* dst = dstBuffer;
            int col = 0; // current column - wraps at imgWidth
            int row = 0; // current row    - increments on wrap

            // Advance the pixel cursor by one position.
            auto advance = [&]() {
                if (++col == imgWidth) {
                    col = 0;
                    ++row;
                }
            };

            // Emit one pixel, respecting the clip rectangle.
            // Used for literal-run pixels where the remap must occur per byte.
            auto emit = [&](unsigned char raw) {
                if (col >= xMin && col <= xMax && row >= yMin && row <= yMax)
                    *dst++ = (raw == 0) ? 0 : paletteRemap[raw];
                advance();
            };

            // PackBits RLE — always, regardless of BMHD compression field.
            bool eof = false;
            unsigned char ctrl;
            while (!eof && fileReadUInt8(stream, &ctrl) != -1) {
                if (ctrl < 0x80) {
                    // Literal run: the next (ctrl + 1) bytes are distinct pixels.
                    int count = ctrl + 1;
                    for (int i = 0; i < count && !eof; i++) {
                        unsigned char px;
                        if (fileReadUInt8(stream, &px) == -1)
                            eof = true;
                        else
                            emit(px);
                    }
                } else {
                    // Run: repeat the next single pixel (257 − ctrl) times.
                    int count = 257 - static_cast<int>(ctrl);
                    unsigned char px;
                    if (fileReadUInt8(stream, &px) == -1) {
                        eof = true;
                    } else {
                        // Remap once for the whole run (mirrors var_10 in the ASM).
                        unsigned char mapped = (px == 0) ? 0 : paletteRemap[px];
                        for (int i = 0; i < count; i++) {
                            if (col >= xMin && col <= xMax && row >= yMin && row <= yMax)
                                *dst++ = mapped;
                            advance();
                        }
                    }
                }
            }

            // Reaching the end of BODY data (normally or via EOF) is the
            // success path.
            fileClose(stream);
            debugPrint("load_lbm_to_buf: loaded %dx%d OK\n", imgWidth, imgHeight);
            return imgWidth;

        } else {
            if (aligned > 0) fileSeek(stream, aligned, SEEK_CUR);
        }
    }

    fileClose(stream);
    return -1;
}

// 0x44F250
int graphCompress(unsigned char* a1, unsigned char* a2, int a3)
{
    _dad_2 = nullptr;
    _rson = nullptr;
    _lson = nullptr;
    _text_buf = nullptr;

    // NOTE: Original code is slightly different, it uses deep nesting or a
    // bunch of gotos.
    _lson = (int*)internal_malloc(sizeof(*_lson) * 4104);
    _rson = (int*)internal_malloc(sizeof(*_rson) * 4376);
    _dad_2 = (int*)internal_malloc(sizeof(*_dad_2) * 4104);
    _text_buf = (unsigned char*)internal_malloc(sizeof(*_text_buf) * 4122);

    if (_lson == nullptr || _rson == nullptr || _dad_2 == nullptr || _text_buf == nullptr) {
        debugPrint("\nGRAPHLIB: Error allocating compression buffers!\n");

        if (_dad_2 != nullptr) {
            internal_free(_dad_2);
        }

        if (_rson != nullptr) {
            internal_free(_rson);
        }
        if (_lson != nullptr) {
            internal_free(_lson);
        }
        if (_text_buf != nullptr) {
            internal_free(_text_buf);
        }

        return -1;
    }

    _InitTree();

    memset(_text_buf, ' ', 4078);

    int count = 0;
    int v30 = 0;
    for (int index = 4078; index < 4096; index++) {
        _text_buf[index] = *a1++;
        int v8 = v30++;
        if (v8 > a3) {
            break;
        }
        count++;
    }

    _textsize = count;

    for (int index = 4077; index > 4059; index--) {
        _InsertNode(index);
    }

    _InsertNode(4078);

    unsigned char v29[32];
    v29[1] = 0;

    int v3 = 4078;
    int v4 = 0;
    int v10 = 0;
    int v36 = 1;
    unsigned char v41 = 1;
    int rc = 0;
    while (count != 0) {
        if (count < _match_length) {
            _match_length = count;
        }

        int v11 = v36 + 1;
        if (_match_length > 2) {
            v29[v36 + 1] = _match_position;
            v29[v36 + 2] = ((_match_length - 3) | ((_match_position >> 4) & 0xF0));
            v36 = v11 + 1;
        } else {
            _match_length = 1;
            v29[1] |= v41;
            int v13 = v36++;
            v29[v13 + 1] = _text_buf[v3];
        }

        v41 *= 2;

        if (v41 == 0) {
            v11 = 0;
            if (v36 != 0) {
                for (;;) {
                    v4++;
                    *a2++ = v29[v11 + 1];
                    if (v4 > a3) {
                        rc = -1;
                        break;
                    }

                    v11++;
                    if (v11 >= v36) {
                        break;
                    }
                }

                if (rc == -1) {
                    break;
                }
            }

            _codesize += v36;
            v29[1] = 0;
            v36 = 1;
            v41 = 1;
        }

        int v16;
        int v38 = _match_length;
        for (v16 = 0; v16 < v38; v16++) {
            unsigned char v34 = *a1++;
            int v17 = v30++;
            if (v17 >= a3) {
                break;
            }

            _DeleteNode(v10);

            unsigned char* v19 = _text_buf + v10;
            _text_buf[v10] = v34;

            if (v10 < 17) {
                v19[4096] = v34;
            }

            v3 = (v3 + 1) & 0xFFF;
            v10 = (v10 + 1) & 0xFFF;
            _InsertNode(v3);
        }

        for (; v16 < v38; v16++) {
            _DeleteNode(v10);
            v3 = (v3 + 1) & 0xFFF;
            v10 = (v10 + 1) & 0xFFF;
            if (--count != 0) {
                _InsertNode(v3);
            }
        }
    }

    if (rc != -1) {
        for (int v23 = 0; v23 < v36; v23++) {
            v4++;
            v10++;
            *a2++ = v29[v23 + 1];
            if (v10 > a3) {
                rc = -1;
                break;
            }
        }

        _codesize += v36;
    }

    internal_free(_lson);
    internal_free(_rson);
    internal_free(_dad_2);
    internal_free(_text_buf);

    if (rc == -1) {
        v4 = -1;
    }

    return v4;
}

// 0x44F5F0
static void _InitTree()
{
    for (int index = 4097; index < 4353; index++) {
        _rson[index] = 4096;
    }

    for (int index = 0; index < 4096; index++) {
        _dad_2[index] = 4096;
    }
}

// 0x44F63C
static void _InsertNode(int a1)
{
    _lson[a1] = 4096;
    _rson[a1] = 4096;
    _match_length = 0;

    unsigned char* v2 = _text_buf + a1;

    int v21 = 4097 + _text_buf[a1];
    int v5 = 1;
    for (;;) {
        int v6 = v21;
        if (v5 < 0) {
            if (_lson[v6] == 4096) {
                _lson[v6] = a1;
                _dad_2[a1] = v21;
                return;
            }
            v21 = _lson[v6];
        } else {
            if (_rson[v6] == 4096) {
                _rson[v6] = a1;
                _dad_2[a1] = v21;
                return;
            }
            v21 = _rson[v6];
        }

        int v9;
        unsigned char* v10 = v2 + 1;
        int v11 = v21 + 1;
        for (v9 = 1; v9 < 18; v9++) {
            v5 = *v10 - _text_buf[v11];
            if (v5 != 0) {
                break;
            }
            v10++;
            v11++;
        }

        if (v9 > _match_length) {
            _match_length = v9;
            _match_position = v21;
            if (v9 >= 18) {
                break;
            }
        }
    }

    _dad_2[a1] = _dad_2[v21];
    _lson[a1] = _lson[v21];
    _rson[a1] = _rson[v21];

    _dad_2[_lson[v21]] = a1;
    _dad_2[_rson[v21]] = a1;

    if (_rson[_dad_2[v21]] == v21) {
        _rson[_dad_2[v21]] = a1;
    } else {
        _lson[_dad_2[v21]] = a1;
    }

    _dad_2[v21] = 4096;
}

// 0x44F7EC
static void _DeleteNode(int a1)
{
    if (_dad_2[a1] != 4096) {
        int v5;
        if (_rson[a1] == 4096) {
            v5 = _lson[a1];
        } else {
            if (_lson[a1] == 4096) {
                v5 = _rson[a1];
            } else {
                v5 = _lson[a1];

                if (_rson[v5] != 4096) {
                    do {
                        v5 = _rson[v5];
                    } while (_rson[v5] != 4096);

                    _rson[_dad_2[v5]] = _lson[v5];
                    _dad_2[_lson[v5]] = _dad_2[v5];
                    _lson[v5] = _lson[a1];
                    _dad_2[_lson[a1]] = v5;
                }

                _rson[v5] = _rson[a1];
                _dad_2[_rson[a1]] = v5;
            }
        }

        _dad_2[v5] = _dad_2[a1];

        if (_rson[_dad_2[a1]] == a1) {
            _rson[_dad_2[a1]] = v5;
        } else {
            _lson[_dad_2[a1]] = v5;
        }
        _dad_2[a1] = 4096;
    }
}

// 0x44F92C
int graphDecompress(unsigned char* src, unsigned char* dest, int length)
{
    _text_buf = (unsigned char*)internal_malloc(sizeof(*_text_buf) * 4122);
    if (_text_buf == nullptr) {
        debugPrint("\nGRAPHLIB: Error allocating decompression buffer!\n");
        return -1;
    }

    int v8 = 4078;
    memset(_text_buf, ' ', v8);

    int v21 = 0;
    int index = 0;
    while (index < length) {
        v21 >>= 1;
        if ((v21 & 0x100) == 0) {
            v21 = *src++;
            v21 |= 0xFF00;
        }

        if ((v21 & 0x01) == 0) {
            int v10 = *src++;
            int v11 = *src++;

            v10 |= (v11 & 0xF0) << 4;
            v11 &= 0x0F;
            v11 += 2;

            for (int v16 = 0; v16 <= v11; v16++) {
                int v17 = (v10 + v16) & 0xFFF;

                unsigned char ch = _text_buf[v17];
                _text_buf[v8] = ch;
                *dest++ = ch;

                v8 = (v8 + 1) & 0xFFF;

                index++;
                if (index >= length) {
                    break;
                }
            }
        } else {
            unsigned char ch = *src++;
            _text_buf[v8] = ch;
            *dest++ = ch;

            v8 = (v8 + 1) & 0xFFF;

            index++;
        }
    }

    internal_free(_text_buf);

    return 0;
}

// 0x44FA78
void grayscalePaletteUpdate(int a1, int a2)
{
    if (a1 >= 0 && a2 <= 255) {
        for (int index = a1; index <= a2; index++) {
            // NOTE: Calls `Color2RGB` many times due to `min` and `max` macro
            // uses.
            int v1 = std::max((Color2RGB(index) & 0x7C00) >> 10, std::max((Color2RGB(index) & 0x3E0) >> 5, Color2RGB(index) & 0x1F));
            int v2 = std::min((Color2RGB(index) & 0x7C00) >> 10, std::min((Color2RGB(index) & 0x3E0) >> 5, Color2RGB(index) & 0x1F));
            int v3 = v1 + v2;
            int v4 = (int)((double)v3 * 240.0 / 510.0);

            int paletteIndex = ((v4 & 0xFF) << 10) | ((v4 & 0xFF) << 5) | (v4 & 0xFF);
            _GreyTable[index] = _colorTable[paletteIndex];
        }
    }
}

// 0x44FC40
void grayscalePaletteApply(unsigned char* buffer, int width, int height, int pitch)
{
    unsigned char* ptr = buffer;
    int skip = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char c = *ptr;
            *ptr++ = _GreyTable[c];
        }
        ptr += skip;
    }
}

} // namespace fallout
