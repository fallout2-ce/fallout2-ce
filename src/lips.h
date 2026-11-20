#ifndef LIPS_H
#define LIPS_H

#include <stddef.h>

#include "sound.h"

namespace fallout {

#define PHONEME_COUNT (42)

enum class LipsFlags : unsigned int {
    None = 0x00,
    Looping = 0x01,
    Playing = 0x02,
};

constexpr inline LipsFlags operator&(LipsFlags lhs, LipsFlags rhs)
{
    return static_cast<LipsFlags>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr inline LipsFlags operator|(LipsFlags lhs, LipsFlags rhs)
{
    return static_cast<LipsFlags>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

constexpr inline LipsFlags operator~(LipsFlags rhs)
{
    return static_cast<LipsFlags>(~static_cast<unsigned int>(rhs));
}

inline LipsFlags& operator&=(LipsFlags& lhs, LipsFlags rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline LipsFlags& operator|=(LipsFlags& lhs, LipsFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

typedef struct SpeechMarker {
    int marker;
    int position;
} SpeechMarker;

typedef struct LipsData {
    int version;
    int field_4;
    LipsFlags flags;
    Sound* sound;
    int field_10;
    void* field_14;
    unsigned char* phonemes;
    int field_1C;
    int startOffset;
    int phonemeCount;
    int field_28;
    int markerCount;
    SpeechMarker* markers;
    int field_34;
    int field_38;
    int field_3C;
    int field_40;
    int field_44;
    int field_48;
    int field_4C;
    char fileName[8];
    char audioExtension[4];
    char textExtension[4];
    char lipExtension[4];
    char field_64[260];
} LipsData;

extern unsigned char gLipsCurrentPhoneme;
extern bool gLipsPhonemeChanged;
extern LipsData gLipsData;

void lipsTicker();
int lipsStart();
int lipsLoad(const char* audioFileName, const char* headFileName);
int lipsFree();

} // namespace fallout

#endif /* LIPS_H */
