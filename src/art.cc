#include "art.h"

#include <lodepng.h>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "animation.h"
#include "content_config.h"
#include "datafile.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "memory.h"
#include "proto.h"
#include "settings.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace fallout {

typedef struct ArtListDescription {
    int flags;
    char name[16];
    char* fileNames; // dynamic array of null terminated strings 13 bytes long each
    void* field_18;
    int fileNamesLength; // number of entries in list
} ArtListDescription;

typedef struct HeadDescription {
    int goodFidgetCount;
    int neutralFidgetCount;
    int badFidgetCount;
} HeadDescription;

static int artReadList(const char* path, char** out_arr, int* out_count);
static int artCacheGetFileSizeImpl(int fid, int* out_size);
static int artCacheReadDataImpl(int fid, int* sizePtr, unsigned char* data);
static void artCacheFreeImpl(void* ptr);
static int artReadFrameData(unsigned char* data, File* stream, int count, int* paddingPtr);
static int artReadHeader(Art* art, File* stream);
static int artGetDataSize(const Art* art);
static int paddingForSize(int size);

// A frame is laid out like [ArtFrame header][pixel bytes][padding].
// These functions return a pointer to the pixel bytes, but must be given a pointer to a frame header,
// not any ArtFrame pointer.
static unsigned char* artFrameData(ArtFrame* frame)
{
    return reinterpret_cast<unsigned char*>(frame) + sizeof(*frame);
}

static const unsigned char* artFrameData(const ArtFrame* frame)
{
    return reinterpret_cast<const unsigned char*>(frame) + sizeof(*frame);
}

// 0x5002D8 str2
static char gDefaultJumpsuitMaleFileName[] = "hmjmps";

// 0x05002E0 aHfjmps
static char gDefaultJumpsuitFemaleFileName[] = "hfjmps";

// 0x5002E8 aHmwarr
static char gDefaultTribalMaleFileName[] = "hmwarr";

// 0x5002F0 aHfprim
static char gDefaultTribalFemaleFileName[] = "hfprim";

// 0x510738 art
static ArtListDescription gArtListDescriptions[OBJ_TYPE_COUNT] = {
    { 0, "items", nullptr, nullptr, 0 },
    { 0, "critters", nullptr, nullptr, 0 },
    { 0, "scenery", nullptr, nullptr, 0 },
    { 0, "walls", nullptr, nullptr, 0 },
    { 0, "tiles", nullptr, nullptr, 0 },
    { 0, "misc", nullptr, nullptr, 0 },
    { 0, "intrface", nullptr, nullptr, 0 },
    { 0, "inven", nullptr, nullptr, 0 },
    { 0, "heads", nullptr, nullptr, 0 },
    { 0, "backgrnd", nullptr, nullptr, 0 },
    { 0, "skilldex", nullptr, nullptr, 0 },
};

// This flag denotes that localized arts should be looked up first. Used
// together with [gArtLanguage].
//
// 0x510898 darn_foreigners
static bool gArtLanguageInitialized = false;

// 0x51089C head1
static const char* _head1 = "gggnnnbbbgnb";

// 0x5108A0 head2
static const char* _head2 = "vfngfbnfvppp";

// Current native look base fid.
//
// 0x5108A4 art_vault_guy_num
int _art_vault_guy_num = 0;

// Base fids for unarmored dude.
//
// Outfit file names:
// - tribal: "hmwarr", "hfprim"
// - jumpsuit: "hmjmps", "hfjmps"
//
// NOTE: This value could have been done with two separate arrays - one for
// tribal look, and one for jumpsuit look. However in this case it would have
// been accessed differently in 0x49F984, which clearly uses look type as an
// index, not gender.
//
// 0x5108A8 art_vault_person_nums
int _art_vault_person_nums[DUDE_NATIVE_LOOK_COUNT][GENDER_COUNT];

// Index of "grid001.frm" in tiles.lst.
//
// 0x5108B8 art_mapper_blank_tile
static int _art_mapper_blank_tile = 1;

// Non-english language name.
//
// This value is used as a directory name to display localized arts.
//
// 0x56C970 darn_foreign_sub_path
static char gArtLanguage[32];

// 0x56C990 art_cache
Cache gArtCache;

// 0x56C9E4 art_name
static char _art_name[COMPAT_MAX_PATH];

// head_info
// 0x56CAE8 head_info
static HeadDescription* gHeadDescriptions;

// anon_alias
// 0x56CAEC anon_alias
static int* _anon_alias;

// artCritterFidShouldRunData
// 0x56CAF0 artCritterFidShouldRunData
static int* gArtCritterFidShoudRunData;

static std::unordered_map<std::string, std::shared_ptr<NamedCacheEntry>> gNamedArtCache;
constexpr int kNamedCacheMaxBytes = 32 * 1024 * 1024; // 32MB soft limit
constexpr size_t kMaxNamedPngPixels = 16 * 1024 * 1024;
static unsigned int gNamedArtCacheMruCounter = 0;
static int gNamedArtCacheCurrentBytes = 0;

// 0x418840
int artInit()
{
    char path[COMPAT_MAX_PATH];
    File* stream;
    char string[200];

    int cacheSize = settings.system.art_cache_size;
    if (!cacheInit(&gArtCache, artCacheGetFileSizeImpl, artCacheReadDataImpl, artCacheFreeImpl, cacheSize << 20)) {
        debugPrint("cache_init failed in art_init\n");
        return -1;
    }

    const char* language = settings.system.language.c_str();
    if (compat_stricmp(language, ENGLISH) != 0) {
        strcpy(gArtLanguage, language);
        gArtLanguageInitialized = true;
    }

    bool critterDbSelected = false;
    for (int objectType = 0; objectType < OBJ_TYPE_COUNT; objectType++) {
        gArtListDescriptions[objectType].flags = 0;
        snprintf(path, sizeof(path), "%s%s%s\\%s.lst", _cd_path_base, "art\\", gArtListDescriptions[objectType].name, gArtListDescriptions[objectType].name);

        if (artReadList(path, &(gArtListDescriptions[objectType].fileNames), &(gArtListDescriptions[objectType].fileNamesLength)) != 0) {
            debugPrint("art_read_lst failed in art_init\n");
            cacheFree(&gArtCache);
            return -1;
        }
    }

    _anon_alias = (int*)internal_malloc(sizeof(*_anon_alias) * gArtListDescriptions[OBJ_TYPE_CRITTER].fileNamesLength);
    if (_anon_alias == nullptr) {
        gArtListDescriptions[OBJ_TYPE_CRITTER].fileNamesLength = 0;
        debugPrint("Out of memory for anon_alias in art_init\n");
        cacheFree(&gArtCache);
        return -1;
    }

    gArtCritterFidShoudRunData = (int*)internal_malloc(sizeof(*gArtCritterFidShoudRunData) * gArtListDescriptions[1].fileNamesLength);
    if (gArtCritterFidShoudRunData == nullptr) {
        gArtListDescriptions[OBJ_TYPE_CRITTER].fileNamesLength = 0;
        debugPrint("Out of memory for artCritterFidShouldRunData in art_init\n");
        cacheFree(&gArtCache);
        return -1;
    }

    for (int critterIndex = 0; critterIndex < gArtListDescriptions[OBJ_TYPE_CRITTER].fileNamesLength; critterIndex++) {
        gArtCritterFidShoudRunData[critterIndex] = 0;
    }

    snprintf(path, sizeof(path), "%s%s%s\\%s.lst", _cd_path_base, "art\\", gArtListDescriptions[OBJ_TYPE_CRITTER].name, gArtListDescriptions[OBJ_TYPE_CRITTER].name);

    stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        debugPrint("Unable to open %s in art_init\n", path);
        cacheFree(&gArtCache);
        return -1;
    }

    // SFALL: Modify player model settings.
    char* jumpsuitMaleFileName = nullptr;
    configGetString(&gContentConfig, CONTENT_CONFIG_START_SECTION, "model_male_default", &jumpsuitMaleFileName, gDefaultJumpsuitMaleFileName);

    char* jumpsuitFemaleFileName = nullptr;
    configGetString(&gContentConfig, CONTENT_CONFIG_START_SECTION, "model_female_default", &jumpsuitFemaleFileName, gDefaultJumpsuitFemaleFileName);

    char* tribalMaleFileName = nullptr;
    configGetString(&gContentConfig, CONTENT_CONFIG_START_SECTION, "model_male", &tribalMaleFileName, gDefaultTribalMaleFileName);

    char* tribalFemaleFileName = nullptr;
    configGetString(&gContentConfig, CONTENT_CONFIG_START_SECTION, "model_female", &tribalFemaleFileName, gDefaultTribalFemaleFileName);

    char* critterFileNames = gArtListDescriptions[OBJ_TYPE_CRITTER].fileNames;
    for (int critterIndex = 0; critterIndex < gArtListDescriptions[OBJ_TYPE_CRITTER].fileNamesLength; critterIndex++) {
        if (compat_stricmp(critterFileNames, jumpsuitMaleFileName) == 0) {
            _art_vault_person_nums[DUDE_NATIVE_LOOK_JUMPSUIT][GENDER_MALE] = critterIndex;
        } else if (compat_stricmp(critterFileNames, jumpsuitFemaleFileName) == 0) {
            _art_vault_person_nums[DUDE_NATIVE_LOOK_JUMPSUIT][GENDER_FEMALE] = critterIndex;
        }

        if (compat_stricmp(critterFileNames, tribalMaleFileName) == 0) {
            _art_vault_person_nums[DUDE_NATIVE_LOOK_TRIBAL][GENDER_MALE] = critterIndex;
            _art_vault_guy_num = critterIndex;
        } else if (compat_stricmp(critterFileNames, tribalFemaleFileName) == 0) {
            _art_vault_person_nums[DUDE_NATIVE_LOOK_TRIBAL][GENDER_FEMALE] = critterIndex;
        }

        critterFileNames += 13;
    }

    for (int critterIndex = 0; critterIndex < gArtListDescriptions[OBJ_TYPE_CRITTER].fileNamesLength; critterIndex++) {
        if (!fileReadString(string, sizeof(string), stream)) {
            break;
        }

        char* sep1 = strchr(string, ',');
        if (sep1 != nullptr) {
            _anon_alias[critterIndex] = atoi(sep1 + 1);

            char* sep2 = strchr(sep1 + 1, ',');
            if (sep2 != nullptr) {
                gArtCritterFidShoudRunData[critterIndex] = atoi(sep2 + 1);
            } else {
                gArtCritterFidShoudRunData[critterIndex] = 0;
            }
        } else {
            _anon_alias[critterIndex] = _art_vault_guy_num;
            gArtCritterFidShoudRunData[critterIndex] = 1;
        }
    }

    fileClose(stream);

    char* tileFileNames = gArtListDescriptions[OBJ_TYPE_TILE].fileNames;
    for (int tileIndex = 0; tileIndex < gArtListDescriptions[OBJ_TYPE_TILE].fileNamesLength; tileIndex++) {
        if (compat_stricmp(tileFileNames, "grid001.frm") == 0) {
            _art_mapper_blank_tile = tileIndex;
        }
        tileFileNames += 13;
    }

    gHeadDescriptions = (HeadDescription*)internal_malloc(sizeof(*gHeadDescriptions) * gArtListDescriptions[OBJ_TYPE_HEAD].fileNamesLength);
    if (gHeadDescriptions == nullptr) {
        gArtListDescriptions[OBJ_TYPE_HEAD].fileNamesLength = 0;
        debugPrint("Out of memory for head_info in art_init\n");
        cacheFree(&gArtCache);
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s%s\\%s.lst", _cd_path_base, "art\\", gArtListDescriptions[OBJ_TYPE_HEAD].name, gArtListDescriptions[OBJ_TYPE_HEAD].name);

    stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        debugPrint("Unable to open %s in art_init\n", path);
        cacheFree(&gArtCache);
        return -1;
    }

    for (int headIndex = 0; headIndex < gArtListDescriptions[OBJ_TYPE_HEAD].fileNamesLength; headIndex++) {
        if (!fileReadString(string, sizeof(string), stream)) {
            break;
        }

        char* sep1 = strchr(string, ',');
        if (sep1 != nullptr) {
            *sep1 = '\0';
        } else {
            sep1 = string;
        }

        char* sep2 = strchr(sep1, ',');
        if (sep2 != nullptr) {
            *sep2 = '\0';
        } else {
            sep2 = sep1;
        }

        gHeadDescriptions[headIndex].goodFidgetCount = atoi(sep1 + 1);

        char* sep3 = strchr(sep2, ',');
        if (sep3 != nullptr) {
            *sep3 = '\0';
        } else {
            sep3 = sep2;
        }

        gHeadDescriptions[headIndex].neutralFidgetCount = atoi(sep2 + 1);

        char* sep4 = strpbrk(sep3 + 1, " ,;\t\n");
        if (sep4 != nullptr) {
            *sep4 = '\0';
        }

        gHeadDescriptions[headIndex].badFidgetCount = atoi(sep3 + 1);
    }

    fileClose(stream);

    return 0;
}

// 0x418EB8
void artReset()
{
}

// 0x418EBC
void artExit()
{
    cacheFree(&gArtCache);

    internal_free(_anon_alias);
    internal_free(gArtCritterFidShoudRunData);

    for (int index = 0; index < OBJ_TYPE_COUNT; index++) {
        internal_free(gArtListDescriptions[index].fileNames);
        gArtListDescriptions[index].fileNames = nullptr;

        internal_free(gArtListDescriptions[index].field_18);
        gArtListDescriptions[index].field_18 = nullptr;
    }

    internal_free(gHeadDescriptions);
}

// 0x418F1C
char* artGetObjectTypeName(int objectType)
{
    return objectType >= OBJ_TYPE_ITEM && objectType < OBJ_TYPE_COUNT ? gArtListDescriptions[objectType].name : nullptr;
}

// 0x418F34
int artIsObjectTypeHidden(int objectType)
{
    return objectType >= OBJ_TYPE_ITEM && objectType < OBJ_TYPE_COUNT ? gArtListDescriptions[objectType].flags & 1 : 0;
}

// 0x409DF0
void artToggleObjectTypeHidden(int objectType)
{
    if (objectType >= 0 && objectType < OBJ_TYPE_COUNT) {
        gArtListDescriptions[objectType].flags ^= 1;
    }
}

// 0x418F7C
int artGetFidgetCount(int headFid)
{
    if (FID_TYPE(headFid) != OBJ_TYPE_HEAD) {
        return 0;
    }

    int head = headFid & 0xFFF;

    if (head > gArtListDescriptions[OBJ_TYPE_HEAD].fileNamesLength) {
        return 0;
    }

    HeadDescription* headDescription = &(gHeadDescriptions[head]);

    int fidget = (headFid & 0xFF0000) >> 16;
    switch (fidget) {
    case FIDGET_GOOD:
        return headDescription->goodFidgetCount;
    case FIDGET_NEUTRAL:
        return headDescription->neutralFidgetCount;
    case FIDGET_BAD:
        return headDescription->badFidgetCount;
    }
    return 0;
}

// 0x418FFC
void artRender(int fid, unsigned char* dest, int width, int height, int pitch)
{
    // NOTE: Original code is different. For unknown reason it directly calls
    // many art functions, for example instead of [artLock] it calls lower level
    // [cacheLock], instead of [artGetWidth] is calls [artGetFrame], then get
    // width from frame's struct field. I don't know if this was intentional or
    // not. I've replaced these calls with higher level functions where
    // appropriate.

    CacheEntry* handle;
    Art* frm = artLock(fid, &handle);
    if (frm == nullptr) {
        return;
    }

    unsigned char* frameData = artGetFrameData(frm, 0, 0);
    int frameWidth = artGetWidth(frm, 0, 0);
    int frameHeight = artGetHeight(frm, 0, 0);

    int remainingWidth = width - frameWidth;
    int remainingHeight = height - frameHeight;
    if (remainingWidth < 0 || remainingHeight < 0) {
        if (height * frameWidth >= width * frameHeight) {
            blitBufferToBufferStretchTrans(frameData,
                frameWidth,
                frameHeight,
                frameWidth,
                dest + pitch * ((height - width * frameHeight / frameWidth) / 2),
                width,
                width * frameHeight / frameWidth,
                pitch);
        } else {
            blitBufferToBufferStretchTrans(frameData,
                frameWidth,
                frameHeight,
                frameWidth,
                dest + (width - height * frameWidth / frameHeight) / 2,
                height * frameWidth / frameHeight,
                height,
                pitch);
        }
    } else {
        blitBufferToBufferTrans(frameData,
            frameWidth,
            frameHeight,
            frameWidth,
            dest + pitch * (remainingHeight / 2) + remainingWidth / 2,
            pitch);
    }

    artUnlock(handle);
}

// mapper2.exe: 0x40A03C
int art_list_str(int fid, char* name)
{
    // TODO: Incomplete.

    return -1;
}

int artListIndex(int objectType, const char* name)
{
    if (objectType < 0 || objectType >= OBJ_TYPE_COUNT) return -1;
    if (gArtListDescriptions[objectType].fileNames == nullptr) return -1;

    char upperName[13] = { 0 };
    strncpy(upperName, name, 12);
    upperName[12] = '\0';
    compat_strupr(upperName);

    int length = gArtListDescriptions[objectType].fileNamesLength;
    const char* fileNames = gArtListDescriptions[objectType].fileNames;

    for (int index = 0; index < length; index++) {
        const char* entry = fileNames + index * 13;

        char upperEntry[13];
        strncpy(upperEntry, entry, 12);
        upperEntry[12] = '\0';
        compat_strupr(upperEntry);

        char* p = upperEntry;
        while (*p && ((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
            p++;
        *p = '\0';

        if (strcmp(upperEntry, upperName) == 0) {
            return index;
        }
    }

    return -1;
}

// 0x419160
Art* artLock(int fid, CacheEntry** handlePtr)
{
    if (handlePtr == nullptr) {
        return nullptr;
    }

    Art* art = nullptr;
    cacheLock(&gArtCache, fid, (void**)&art, handlePtr);
    return art;
}

// 0x419188
unsigned char* artLockFrameData(int fid, int frame, int direction, CacheEntry** handlePtr)
{
    Art* art;
    ArtFrame* frm;

    art = nullptr;
    if (handlePtr) {
        cacheLock(&gArtCache, fid, (void**)&art, handlePtr);
    }

    if (art != nullptr) {
        frm = artGetFrame(art, frame, direction);
        if (frm != nullptr) {

            return artFrameData(frm);
        }
    }

    return nullptr;
}

// 0x419260
int artUnlock(CacheEntry* handle)
{
    return cacheUnlock(&gArtCache, handle);
}

// 0x41927C
int artCacheFlush()
{
    return cacheFlush(&gArtCache);
}

// 0x4192B0
int artCopyFileName(int objectType, int id, char* dest)
{
    ArtListDescription* ptr;

    if (objectType < OBJ_TYPE_ITEM || objectType >= OBJ_TYPE_COUNT) {
        return -1;
    }

    ptr = &(gArtListDescriptions[objectType]);

    if (id >= ptr->fileNamesLength) {
        return -1;
    }

    strcpy(dest, ptr->fileNames + id * 13);

    return 0;
}

// 0x419314
int _art_get_code(int animation, int weaponType, char* weaponCodePtr, char* animationCodePtr)
{
    if (weaponType < 0 || weaponType >= WEAPON_ANIMATION_COUNT) {
        return -1;
    }

    if (animation >= ANIM_TAKE_OUT && animation <= ANIM_FIRE_CONTINUOUS) {
        *animationCodePtr = 'c' + (animation - ANIM_TAKE_OUT);
        if (weaponType == WEAPON_ANIMATION_NONE) {
            return -1;
        }

        *weaponCodePtr = 'd' + (weaponType - 1);
        return 0;
    } else if (animation == ANIM_PRONE_TO_STANDING) {
        *animationCodePtr = 'h';
        *weaponCodePtr = 'c';
        return 0;
    } else if (animation == ANIM_BACK_TO_STANDING) {
        *animationCodePtr = 'j';
        *weaponCodePtr = 'c';
        return 0;
    } else if (animation == ANIM_CALLED_SHOT_PIC) {
        *animationCodePtr = 'a';
        *weaponCodePtr = 'n';
        return 0;
    } else if (animation >= FIRST_SF_DEATH_ANIM) {
        *animationCodePtr = 'a' + (animation - FIRST_SF_DEATH_ANIM);
        *weaponCodePtr = 'r';
        return 0;
    } else if (animation >= FIRST_KNOCKDOWN_AND_DEATH_ANIM) {
        *animationCodePtr = 'a' + (animation - FIRST_KNOCKDOWN_AND_DEATH_ANIM);
        *weaponCodePtr = 'b';
        return 0;
    } else if (animation == ANIM_THROW_ANIM) {
        if (weaponType == WEAPON_ANIMATION_KNIFE) {
            // knife
            *weaponCodePtr = 'd';
            *animationCodePtr = 'm';
        } else if (weaponType == WEAPON_ANIMATION_SPEAR) {
            // spear
            *weaponCodePtr = 'g';
            *animationCodePtr = 'm';
        } else {
            // other -> probably rock or grenade
            *weaponCodePtr = 'a';
            *animationCodePtr = 's';
        }
        return 0;
    } else if (animation == ANIM_DODGE_ANIM) {
        if (weaponType <= 0) {
            *weaponCodePtr = 'a';
            *animationCodePtr = 'n';
        } else {
            *weaponCodePtr = 'd' + (weaponType - 1);
            *animationCodePtr = 'e';
        }
        return 0;
    }

    *animationCodePtr = 'a' + animation;
    if (animation <= ANIM_WALK && weaponType > 0) {
        *weaponCodePtr = 'd' + (weaponType - 1);
        return 0;
    }
    *weaponCodePtr = 'a';

    return 0;
}

// 0x419428
char* artBuildFilePath(int fid)
{
    int baseFid = fid;
    int rotation = FID_ROTATION(fid);

    int aliasFid = artAliasFid(fid);
    if (aliasFid != -1) {
        baseFid = aliasFid;
    }

    *_art_name = '\0';

    int frmId = baseFid & 0xFFF;
    int animType = FID_ANIM_TYPE(baseFid);
    int weaponCode = (baseFid & 0xF000) >> 12;
    int objectType = FID_TYPE(baseFid);

    if (objectType < OBJ_TYPE_ITEM || objectType >= OBJ_TYPE_COUNT) {
        return nullptr;
    }

    if (frmId >= gArtListDescriptions[objectType].fileNamesLength) {
        return nullptr;
    }

    int fileNameOffset = frmId * 13;

    if (objectType == OBJ_TYPE_CRITTER) {
        char critterWeaponCode;
        char critterAnimationCode;
        if (_art_get_code(animType, weaponCode, &critterWeaponCode, &critterAnimationCode) == -1) {
            return nullptr;
        }
        if (rotation != 0) {
            snprintf(_art_name, sizeof(_art_name), "%s%s%s\\%s%c%c.fr%c", _cd_path_base, "art\\", gArtListDescriptions[OBJ_TYPE_CRITTER].name, gArtListDescriptions[OBJ_TYPE_CRITTER].fileNames + fileNameOffset, critterWeaponCode, critterAnimationCode, rotation + 47);
        } else {
            snprintf(_art_name, sizeof(_art_name), "%s%s%s\\%s%c%c.frm", _cd_path_base, "art\\", gArtListDescriptions[OBJ_TYPE_CRITTER].name, gArtListDescriptions[OBJ_TYPE_CRITTER].fileNames + fileNameOffset, critterWeaponCode, critterAnimationCode);
        }
    } else if (objectType == OBJ_TYPE_HEAD) {
        char headSuffix = _head2[animType];
        if (headSuffix == 'f') {
            snprintf(_art_name, sizeof(_art_name), "%s%s%s\\%s%c%c%d.frm", _cd_path_base, "art\\", gArtListDescriptions[OBJ_TYPE_HEAD].name, gArtListDescriptions[OBJ_TYPE_HEAD].fileNames + fileNameOffset, _head1[animType], 102, weaponCode);
        } else {
            snprintf(_art_name, sizeof(_art_name), "%s%s%s\\%s%c%c.frm", _cd_path_base, "art\\", gArtListDescriptions[OBJ_TYPE_HEAD].name, gArtListDescriptions[OBJ_TYPE_HEAD].fileNames + fileNameOffset, _head1[animType], headSuffix);
        }
    } else {
        snprintf(_art_name, sizeof(_art_name), "%s%s%s\\%s", _cd_path_base, "art\\", gArtListDescriptions[objectType].name, gArtListDescriptions[objectType].fileNames + fileNameOffset);
    }

    return _art_name;
}

// art_read_lst
// 0x419664
static int artReadList(const char* path, char** artListPtr, int* artListSizePtr)
{
    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    int count = 0;
    char string[200];
    while (fileReadString(string, sizeof(string), stream)) {
        count++;
    }

    fileSeek(stream, 0, SEEK_SET);

    *artListSizePtr = count;

    char* artList = (char*)internal_malloc(13 * count);
    *artListPtr = artList;
    if (artList == nullptr) {
        fileClose(stream);
        return -1;
    }

    while (fileReadString(string, sizeof(string), stream)) {
        char* brk = strpbrk(string, " ,;\r\t\n");
        if (brk != nullptr) {
            *brk = '\0';
        }

        strncpy(artList, string, 12);
        artList[12] = '\0';

        artList += 13;

        count--;
    }

    // Sanity check. There was a bug with uncompressed database file seek
    assert(count == 0);

    fileClose(stream);

    return 0;
}

// 0x419760
int artGetFramesPerSecond(Art* art)
{
    if (art == nullptr) {
        return 10;
    }

    return art->framesPerSecond == 0 ? 10 : art->framesPerSecond;
}

// 0x419778
int artGetActionFrame(Art* art)
{
    return art == nullptr ? -1 : art->actionFrame;
}

// 0x41978C
int artGetFrameCount(Art* art)
{
    return art == nullptr ? -1 : art->frameCount;
}

// 0x4197A0
int artGetWidth(Art* art, int frame, int direction)
{
    ArtFrame* frm;

    frm = artGetFrame(art, frame, direction);
    if (frm == nullptr) {
        return -1;
    }

    return frm->width;
}

// 0x4197B8
int artGetHeight(Art* art, int frame, int direction)
{
    ArtFrame* frm;

    frm = artGetFrame(art, frame, direction);
    if (frm == nullptr) {
        return -1;
    }

    return frm->height;
}

// 0x4197D4
int artGetSize(Art* art, int frame, int direction, int* widthPtr, int* heightPtr)
{
    ArtFrame* frm;

    frm = artGetFrame(art, frame, direction);
    if (frm == nullptr) {
        if (widthPtr != nullptr) {
            *widthPtr = 0;
        }

        if (heightPtr != nullptr) {
            *heightPtr = 0;
        }

        return -1;
    }

    if (widthPtr != nullptr) {
        *widthPtr = frm->width;
    }

    if (heightPtr != nullptr) {
        *heightPtr = frm->height;
    }

    return 0;
}

// 0x419820
int artGetFrameOffsets(const Art* art, int frame, int direction, int* xPtr, int* yPtr)
{
    return artGetFrameData(art, frame, direction, nullptr, nullptr, xPtr, yPtr) != nullptr ? 0 : -1;
}

// 0x41984C
int artGetRotationOffsets(Art* art, int rotation, int* xPtr, int* yPtr)
{
    if (art == nullptr) {
        return -1;
    }

    *xPtr = art->xOffsets[rotation];
    *yPtr = art->yOffsets[rotation];

    return 0;
}

// 0x419870
unsigned char* artGetFrameData(Art* art, int frame, int direction)
{
    return artGetFrameData(art, frame, direction, nullptr, nullptr, nullptr, nullptr);
}

unsigned char* artGetFrameData(const Art* art, int frame, int direction, int* widthPtr, int* heightPtr, int* xOffsetPtr, int* yOffsetPtr)
{
    ArtFrame* frm = artGetFrame(art, frame, direction);
    if (frm == nullptr) {
        return nullptr;
    }

    if (widthPtr != nullptr) {
        *widthPtr = frm->width;
    }

    if (heightPtr != nullptr) {
        *heightPtr = frm->height;
    }

    if (xOffsetPtr != nullptr) {
        *xOffsetPtr = frm->x;
    }

    if (yOffsetPtr != nullptr) {
        *yOffsetPtr = frm->y;
    }

    return artFrameData(frm);
}

// 0x419880
ArtFrame* artGetFrame(const Art* art, int frame, int rotation)
{
    if (rotation < 0 || rotation >= 6) {
        return nullptr;
    }

    if (art == nullptr) {
        return nullptr;
    }

    if (frame < 0 || frame >= art->frameCount) {
        return nullptr;
    }

    ArtFrame* frm = (ArtFrame*)((unsigned char*)art + sizeof(*art) + art->dataOffsets[rotation] + art->padding[rotation]);
    for (int index = 0; index < frame; index++) {
        frm = (ArtFrame*)((unsigned char*)frm + sizeof(*frm) + frm->size + paddingForSize(frm->size));
    }
    return frm;
}

ConstBuffer2D artGetFrameBuffer(const Art* art, int frame, int direction)
{
    int width = 0;
    int height = 0;
    unsigned char* data = artGetFrameData(art, frame, direction, &width, &height, nullptr, nullptr);
    return { data, width, height };
}

// 0x4198C8
bool artExists(int fid)
{
    bool result = false;

    char* filePath = artBuildFilePath(fid);
    if (filePath != nullptr) {
        int fileSize;
        if (dbGetFileSize(filePath, &fileSize) != -1) {
            result = true;
        }
    }

    return result;
}

// NOTE: Exactly the same implementation as `artExists`.
//
// 0x419930
bool _art_fid_valid(int fid)
{
    bool result = false;

    char* filePath = artBuildFilePath(fid);
    if (filePath != nullptr) {
        int fileSize;
        if (dbGetFileSize(filePath, &fileSize) != -1) {
            result = true;
        }
    }

    return result;
}

// 0x419998
int _art_alias_num(int index)
{
    return _anon_alias[index];
}

// 0x4199AC
int artCritterFidShouldRun(int fid)
{
    if (FID_TYPE(fid) == OBJ_TYPE_CRITTER) {
        return gArtCritterFidShoudRunData[fid & 0xFFF];
    }

    return 0;
}

// 0x4199D4
int artAliasFid(int fid)
{
    int type = FID_TYPE(fid);
    int anim = FID_ANIM_TYPE(fid);
    if (type == OBJ_TYPE_CRITTER) {
        if (anim == ANIM_ELECTRIFY
            || anim == ANIM_BURNED_TO_NOTHING
            || anim == ANIM_ELECTRIFIED_TO_NOTHING
            || anim == ANIM_ELECTRIFY_SF
            || anim == ANIM_BURNED_TO_NOTHING_SF
            || anim == ANIM_ELECTRIFIED_TO_NOTHING_SF
            || anim == ANIM_FIRE_DANCE
            || anim == ANIM_CALLED_SHOT_PIC) {
            // NOTE: Original code is slightly different. It uses many mutually
            // mirrored bitwise operators. Probably result of some macros for
            // getting/setting individual bits on fid.
            return (fid & 0x70000000) | ((anim << 16) & 0xFF0000) | 0x1000000 | (fid & 0xF000) | (_anon_alias[fid & 0xFFF] & 0xFFF);
        }
    }

    return -1;
}

static bool artGetLocalizedPath(const char* basePath, const char** outPath)
{
    static char localizedPath[COMPAT_MAX_PATH];

    if (!gArtLanguageInitialized) {
        return false;
    }
    const char* pch = strchr(basePath, '\\');
    if (pch == nullptr) {
        pch = basePath;
    }
    snprintf(localizedPath, sizeof(localizedPath), "art\\%s\\%s", gArtLanguage, pch);
    *outPath = localizedPath;
    return true;
}

// 0x419A78
static int artCacheGetFileSizeImpl(int fid, int* sizePtr)
{
    int result = -1;

    char* artFilePath = artBuildFilePath(fid);
    if (artFilePath != nullptr) {
        File* stream = nullptr;
        const char* localizedPath;
        if (artGetLocalizedPath(artFilePath, &localizedPath)) {
            stream = fileOpen(localizedPath, "rb");
        }
        if (stream == nullptr) {
            stream = fileOpen(artFilePath, "rb");
        }

        if (stream != nullptr) {
            Art art;
            if (artReadHeader(&art, stream) == 0) {
                *sizePtr = artGetDataSize(&art);
                result = 0;
            }
            fileClose(stream);
        }
    }

    return result;
}

// 0x419B78
static int artCacheReadDataImpl(int fid, int* sizePtr, unsigned char* data)
{
    int result = -1;

    char* artFileName = artBuildFilePath(fid);
    if (artFileName != nullptr) {
        bool loaded = false;
        const char* localizedPath;
        if (artGetLocalizedPath(artFileName, &localizedPath)) {
            if (artRead(localizedPath, data) == 0) {
                loaded = true;
            }
        }

        if (!loaded) {
            if (artRead(artFileName, data) == 0) {
                loaded = true;
            }
        }

        if (loaded) {
            *sizePtr = artGetDataSize((Art*)data);
            result = 0;
        }
    }

    return result;
}

// 0x419C80
static void artCacheFreeImpl(void* ptr)
{
    internal_free(ptr);
}

/* FID Structure:
    3 bits for rotation
    4 bits for object type
    8 bits for animation type
    4 bits for weapon code
    12 bits for frame ID
*/
static int buildFidInternal(unsigned short frmId, unsigned char weaponCode, unsigned char animType, unsigned char objectType, unsigned char rotation)
{
    return ((rotation << 28) & 0x70000000) | (objectType << 24) | ((animType << 16) & 0xFF0000) | ((weaponCode << 12) & 0xF000) | (frmId & 0xFFF);
}

// 0x419C88
int buildFid(int objectType, int frmId, int animType, int weaponCode, int rotation)
{
    // Always use rotation 0 (NE) for non-critters, for certain critter animations.
    // For other critter animations, check if art for the given rotation exists, if not try rotation 1 (E) and if that also doesn't exist, then default to 0 (NE).
    if (objectType != OBJ_TYPE_CRITTER
        || animType == ANIM_FIRE_DANCE
        || animType < ANIM_FALL_BACK
        || animType > ANIM_FALL_FRONT_BLOOD) {
        rotation = ROTATION_NE;
    } else if (!artExists(buildFidInternal(frmId, weaponCode, animType, OBJ_TYPE_CRITTER, rotation))) {
        rotation = rotation != ROTATION_E
                && artExists(buildFidInternal(frmId, weaponCode, animType, OBJ_TYPE_CRITTER, ROTATION_E))
            ? ROTATION_E
            : ROTATION_NE;
    }
    return buildFidInternal(frmId, weaponCode, animType, objectType, rotation);
}

// 0x419D60
static int artReadFrameData(unsigned char* data, File* stream, int count, int* paddingPtr)
{
    unsigned char* ptr = data;
    int padding = 0;
    for (int index = 0; index < count; index++) {
        ArtFrame* frame = (ArtFrame*)ptr;

        if (fileReadInt16(stream, &(frame->width)) == -1) return -1;
        if (fileReadInt16(stream, &(frame->height)) == -1) return -1;
        if (fileReadInt32(stream, &(frame->size)) == -1) return -1;
        if (fileReadInt16(stream, &(frame->x)) == -1) return -1;
        if (fileReadInt16(stream, &(frame->y)) == -1) return -1;
        if (fileRead(ptr + sizeof(ArtFrame), frame->size, 1, stream) != 1) return -1;

        ptr += sizeof(ArtFrame) + frame->size;
        ptr += paddingForSize(frame->size);
        padding += paddingForSize(frame->size);
    }

    *paddingPtr = padding;

    return 0;
}

// 0x419E1C
static int artReadHeader(Art* art, File* stream)
{
    if (fileReadInt32(stream, &(art->version)) == -1) return -1;
    if (fileReadInt16(stream, &(art->framesPerSecond)) == -1) return -1;
    if (fileReadInt16(stream, &(art->actionFrame)) == -1) return -1;
    if (fileReadInt16(stream, &(art->frameCount)) == -1) return -1;
    if (fileReadInt16List(stream, art->xOffsets, ROTATION_COUNT) == -1) return -1;
    if (fileReadInt16List(stream, art->yOffsets, ROTATION_COUNT) == -1) return -1;
    if (fileReadInt32List(stream, art->dataOffsets, ROTATION_COUNT) == -1) return -1;
    if (fileReadInt32(stream, &(art->dataSize)) == -1) return -1;

    // CE: Fix malformed `frm` files with `dataSize` set to 0 in Nevada.
    if (art->dataSize == 0) {
        art->dataSize = fileGetSize(stream);
    }

    return 0;
}

static bool artPathHasExtension(const char* path, const char* extension)
{
    size_t pathLength = strlen(path);
    size_t extensionLength = strlen(extension);
    if (pathLength < extensionLength) {
        return false;
    }

    return compat_stricmp(path + pathLength - extensionLength, extension) == 0;
}

static bool artReadFile(const char* path, std::vector<unsigned char>& data)
{
    int size = 0;
    if (dbGetFileSize(path, &size) != 0 || size <= 0) {
        return false;
    }

    data.resize(size);
    return dbGetFileContents(path, data.data()) == 0;
}

static bool artUnpackIndexedPngPixels(const std::vector<unsigned char>& indexedData, unsigned width, unsigned height, unsigned bitdepth, unsigned char* output)
{
    if (bitdepth != 8) {
        return false;
    }

    size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (indexedData.size() < pixelCount) {
        return false;
    }

    memcpy(output, indexedData.data(), pixelCount);
    return true;
}

static bool artValidateIndexedPngHeader(const char* path, unsigned width, unsigned height, const LodePNGColorMode& color)
{
    if (color.colortype != LCT_PALETTE) {
        debugPrint("ART: PNG is not palette-indexed: %s\n", path);
        return false;
    }

    if (color.bitdepth != 8) {
        debugPrint("ART: indexed PNG bit depth must be 8: %s\n", path);
        return false;
    }

    if (width == 0 || height == 0 || width > SHRT_MAX || height > SHRT_MAX) {
        debugPrint("ART: invalid indexed PNG dimensions for %s: %ux%u\n", path, width, height);
        return false;
    }

    size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixelCount > kMaxNamedPngPixels || pixelCount > INT_MAX) {
        debugPrint("ART: indexed PNG is too large: %s\n", path);
        return false;
    }

    return true;
}

static bool artIndexedPngHasSupportedTransparency(const LodePNGColorMode& color)
{
    if (!lodepng_has_palette_alpha(&color)) {
        return true;
    }

    for (size_t index = 0; index < color.palettesize; index++) {
        unsigned char alpha = color.palette[index * 4 + 3];
        if (index == 0) {
            // palette index 0 is allowed to be either fully transparent or fully opaque
            if (alpha != 0 && alpha != 255) {
                return false;
            }
        } else if (alpha != 255) {
            return false;
        }
    }

    return true;
}

static Art* artAllocateSingleFrame(int width, int height, unsigned char** frameDataPtr)
{
    if (width <= 0 || height <= 0 || width > SHRT_MAX || height > SHRT_MAX) {
        return nullptr;
    }

    size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixelCount > kMaxNamedPngPixels || pixelCount > INT_MAX) {
        return nullptr;
    }

    if (pixelCount > static_cast<size_t>(INT_MAX) - sizeof(ArtFrame)) {
        return nullptr;
    }

    Art header = {};
    header.version = 4;
    header.framesPerSecond = 10;
    header.actionFrame = 0;
    header.frameCount = 1;
    // FRM dataSize excludes in-memory alignment padding; artGetDataSize adds
    // the padding needed by artGetFrame's adjusted frame offsets.
    header.dataSize = static_cast<int>(sizeof(ArtFrame) + pixelCount);

    int currentPadding = paddingForSize(sizeof(Art));
    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        header.dataOffsets[rotation] = 0;
        header.padding[rotation] = currentPadding;
    }

    int dataSize = artGetDataSize(&header);
    unsigned char* data = reinterpret_cast<unsigned char*>(internal_malloc(dataSize));
    if (data == nullptr) {
        return nullptr;
    }

    memset(data, 0, dataSize);
    Art* art = reinterpret_cast<Art*>(data);
    *art = header;

    ArtFrame* frame = reinterpret_cast<ArtFrame*>(data + sizeof(Art) + art->padding[0]);
    frame->width = static_cast<short>(width);
    frame->height = static_cast<short>(height);
    frame->size = static_cast<int>(pixelCount);
    frame->x = 0;
    frame->y = 0;

    *frameDataPtr = artFrameData(frame);
    return art;
}

static Art* artLoadIndexedPng(const char* path)
{
    std::vector<unsigned char> encoded;
    if (!artReadFile(path, encoded)) {
        return nullptr;
    }

    lodepng::State state;
    state.decoder.color_convert = 0;

    unsigned width = 0;
    unsigned height = 0;
    unsigned error = lodepng_inspect(&width, &height, &state, encoded.data(), encoded.size());
    if (error != 0) {
        debugPrint("ART: failed to inspect indexed PNG %s: %s\n", path, lodepng_error_text(error));
        return nullptr;
    }

    if (!artValidateIndexedPngHeader(path, width, height, state.info_png.color)) {
        return nullptr;
    }

    std::vector<unsigned char> indexedData;
    error = lodepng::decode(indexedData, width, height, state, encoded);
    if (error != 0) {
        debugPrint("ART: failed to decode indexed PNG %s: %s\n", path, lodepng_error_text(error));
        return nullptr;
    }

    if (!artIndexedPngHasSupportedTransparency(state.info_png.color)) {
        debugPrint("ART: indexed PNG transparency is unsupported, reserve palette index 0 instead: %s\n", path);
        return nullptr;
    }

    unsigned char* frameData = nullptr;
    Art* art = artAllocateSingleFrame(static_cast<int>(width), static_cast<int>(height), &frameData);
    if (art == nullptr) {
        return nullptr;
    }

    if (!artUnpackIndexedPngPixels(indexedData, width, height, state.info_png.color.bitdepth, frameData)) {
        debugPrint("ART: failed to read indexed PNG pixels: %s\n", path);
        internal_free(art);
        return nullptr;
    }

    return art;
}

static Art* artLoadPcx(const char* path)
{
    char mutablePath[COMPAT_MAX_PATH];
    strncpy(mutablePath, path, sizeof(mutablePath));
    mutablePath[sizeof(mutablePath) - 1] = '\0';

    int width = 0;
    int height = 0;
    unsigned char* pcxData = datafileRead(mutablePath, &width, &height);
    if (pcxData == nullptr) {
        return nullptr;
    }

    unsigned char* frameData = nullptr;
    Art* art = artAllocateSingleFrame(width, height, &frameData);
    if (art != nullptr) {
        memcpy(frameData, pcxData, static_cast<size_t>(width) * static_cast<size_t>(height));
    }

    internal_free(pcxData);
    return art;
}

static Art* artLoadFrm(const char* path)
{
    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        return nullptr;
    }

    Art header;
    if (artReadHeader(&header, stream) != 0) {
        fileClose(stream);
        return nullptr;
    }

    fileClose(stream);

    unsigned char* data = reinterpret_cast<unsigned char*>(internal_malloc(artGetDataSize(&header)));
    if (data == nullptr) {
        return nullptr;
    }

    if (artRead(path, data) != 0) {
        internal_free(data);
        return nullptr;
    }

    return reinterpret_cast<Art*>(data);
}

// NOTE: Original function was slightly different, but never used. Basically
// it's a memory allocating variant of `artRead` (which reads data into given
// buffer). This function is useful to load custom `frm` files since `Art` now
// needs more memory then it's on-disk size (due to memory padding).
//
// 0x419EC0
Art* artLoad(const char* path)
{
    if (path == nullptr) {
        return nullptr;
    }

    if (artPathHasExtension(path, ".png")) {
        return artLoadIndexedPng(path);
    }

    if (artPathHasExtension(path, ".pcx")) {
        return artLoadPcx(path);
    }

    Art* art = artLoadFrm(path);
    if (art != nullptr) {
        return art;
    }

    return nullptr;
}

static Art* artLoadLocalized(const char* path)
{
    const char* localizedPath;
    Art* result = artGetLocalizedPath(path, &localizedPath)
        ? artLoad(localizedPath)
        : nullptr;

    return result != nullptr ? result : artLoad(path);
}

// 0x419FC0
int artRead(const char* path, unsigned char* data)
{
    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        return -2;
    }

    Art* art = (Art*)data;
    if (artReadHeader(art, stream) != 0) {
        fileClose(stream);
        return -3;
    }

    int currentPadding = paddingForSize(sizeof(Art));
    int previousPadding = 0;

    for (int index = 0; index < ROTATION_COUNT; index++) {
        art->padding[index] = currentPadding;

        if (index == 0 || art->dataOffsets[index - 1] != art->dataOffsets[index]) {
            art->padding[index] += previousPadding;
            currentPadding += previousPadding;
            if (artReadFrameData(data + sizeof(Art) + art->dataOffsets[index] + art->padding[index], stream, art->frameCount, &previousPadding) != 0) {
                fileClose(stream);
                return -5;
            }
        }
    }

    fileClose(stream);
    return 0;
}

// NOTE: Unused.
//
// 0x41A070
int artWriteFrameData(unsigned char* data, File* stream, int count)
{
    unsigned char* ptr = data;
    for (int index = 0; index < count; index++) {
        ArtFrame* frame = (ArtFrame*)ptr;

        if (fileWriteInt16(stream, frame->width) == -1) return -1;
        if (fileWriteInt16(stream, frame->height) == -1) return -1;
        if (fileWriteInt32(stream, frame->size) == -1) return -1;
        if (fileWriteInt16(stream, frame->x) == -1) return -1;
        if (fileWriteInt16(stream, frame->y) == -1) return -1;
        if (fileWrite(ptr + sizeof(ArtFrame), frame->size, 1, stream) != 1) return -1;

        ptr += sizeof(ArtFrame) + frame->size;
        ptr += paddingForSize(frame->size);
    }

    return 0;
}

// NOTE: Unused.
//
// 0x41A138
int artWriteHeader(Art* art, File* stream)
{
    if (fileWriteInt32(stream, art->version) == -1) return -1;
    if (fileWriteInt16(stream, art->framesPerSecond) == -1) return -1;
    if (fileWriteInt16(stream, art->actionFrame) == -1) return -1;
    if (fileWriteInt16(stream, art->frameCount) == -1) return -1;
    if (fileWriteInt16List(stream, art->xOffsets, ROTATION_COUNT) == -1) return -1;
    if (fileWriteInt16List(stream, art->yOffsets, ROTATION_COUNT) == -1) return -1;
    if (fileWriteInt32List(stream, art->dataOffsets, ROTATION_COUNT) == -1) return -1;
    if (fileWriteInt32(stream, art->dataSize) == -1) return -1;

    return 0;
}

// NOTE: Unused.
//
// 0x41A1E8
int artWrite(const char* path, unsigned char* data)
{
    if (data == nullptr) {
        return -1;
    }

    File* stream = fileOpen(path, "wb");
    if (stream == nullptr) {
        return -1;
    }

    Art* art = (Art*)data;
    if (artWriteHeader(art, stream) == -1) {
        fileClose(stream);
        return -1;
    }

    for (int index = 0; index < ROTATION_COUNT; index++) {
        if (index == 0 || art->dataOffsets[index - 1] != art->dataOffsets[index]) {
            if (artWriteFrameData(data + sizeof(Art) + art->dataOffsets[index] + art->padding[index], stream, art->frameCount) != 0) {
                fileClose(stream);
                return -1;
            }
        }
    }

    fileClose(stream);
    return 0;
}

static int artGetDataSize(const Art* art)
{
    int dataSize = sizeof(*art) + art->dataSize;

    for (int index = 0; index < ROTATION_COUNT; index++) {
        if (index == 0 || art->dataOffsets[index - 1] != art->dataOffsets[index]) {
            // Assume worst case - every frame is unaligned and need
            // max padding.
            dataSize += (sizeof(int) - 1) * art->frameCount;
        }
    }

    return dataSize;
}

static int paddingForSize(int size)
{
    return (sizeof(int) - size % sizeof(int)) % sizeof(int);
}

class NamedCacheEntry {
public:
    explicit NamedCacheEntry(ArtPtr&& art);

    const Art* art() const { return _art.get(); }

    unsigned int mru = 0;

private:
    ArtPtr _art;
};

NamedCacheEntry::NamedCacheEntry(ArtPtr&& art)
    : _art(std::move(art))
{
}

std::shared_ptr<NamedCacheEntry> artLockNamedFrameData(const char* path)
{
    auto it = gNamedArtCache.find(path);
    if (it != gNamedArtCache.end()) {
        it->second->mru = ++gNamedArtCacheMruCounter;
        return it->second;
    }

    Art* art = artLoadLocalized(path);
    if (!art) return nullptr;

    if (gNamedArtCacheMruCounter == UINT_MAX) {
        // This looks complicated, but it should happen rarely and needed to preserve mru order.
        std::vector<NamedCacheEntry*> sorted;
        sorted.reserve(gNamedArtCache.size());
        for (auto& [key, e] : gNamedArtCache) {
            sorted.push_back(e.get());
        }
        std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) { return a->mru < b->mru; });
        unsigned int mru = 0;
        for (auto* e : sorted) {
            e->mru = mru++;
        }
        gNamedArtCacheMruCounter = mru;
    }

    auto entry = std::make_shared<NamedCacheEntry>(ArtPtr(art));
    entry->mru = ++gNamedArtCacheMruCounter;

    gNamedArtCacheCurrentBytes += artGetDataSize(art);

    // Evict LRU entries if over soft limit (post-insertion)
    while (gNamedArtCacheCurrentBytes > kNamedCacheMaxBytes) {
        unsigned int lowestMru = UINT_MAX;
        auto evictIt = gNamedArtCache.end();
        for (auto iter = gNamedArtCache.begin(); iter != gNamedArtCache.end(); ++iter) {
            if (iter->second.use_count() == 1 && iter->second->mru < lowestMru) {
                lowestMru = iter->second->mru;
                evictIt = iter;
            }
        }

        if (evictIt == gNamedArtCache.end()) {
            break;
        }

        gNamedArtCacheCurrentBytes -= artGetDataSize(evictIt->second->art());
        gNamedArtCache.erase(evictIt);
    }

    return gNamedArtCache.emplace(path, std::move(entry)).first->second;
}

FrmId::FrmId(ObjectType objType, int frmId)
    : _fid(buildFid(objType, frmId, 0, 0, 0))
{
    assert(objType >= 0 && objType < OBJ_TYPE_COUNT);
}

FrmId::FrmId(ObjectType objType, const char* path)
    : _objectType(objType)
    , _path(path)
{
    assert(objType >= 0 && objType < OBJ_TYPE_COUNT);
}

FrmId::FrmId(const char* path)
    : _path(path)
{
}

ObjectType FrmId::objectType() const
{
    assert(hasObjectType());
    return static_cast<ObjectType>(_objectType);
}

FrmImage::FrmImage()
{
    _key = nullptr;
    _data = nullptr;
    _width = 0;
    _height = 0;
    _xOffset = 0;
    _yOffset = 0;
}

FrmImage::~FrmImage()
{
    unlock();
}

FrmImage::FrmImage(FrmImage&& other) noexcept
    : _namedKey(std::move(other._namedKey))
    , _key(other._key)
    , _data(other._data)
    , _width(other._width)
    , _height(other._height)
    , _xOffset(other._xOffset)
    , _yOffset(other._yOffset)
{
    other.resetInternal();
}

FrmImage& FrmImage::operator=(FrmImage&& other) noexcept
{
    if (this != &other) {
        unlock();
        _namedKey = std::move(other._namedKey);
        _key = other._key;
        _data = other._data;
        _width = other._width;
        _height = other._height;
        _xOffset = other._xOffset;
        _yOffset = other._yOffset;

        other.resetInternal();
    }
    return *this;
}

bool FrmImage::lock(const FrmId& frmId)
{
    return lock(frmId, 0, 0);
}

bool FrmImage::lock(const FrmId& frmId, int frame, int direction)
{
    if (frmId.fid() >= 0) {
        return lock(frmId.fid(), frame, direction);
    }
    if (frmId.filePath() != nullptr) {
        return frmId.hasObjectType()
            ? lock(frmId.objectType(), frmId.filePath(), frame, direction)
            : lock(frmId.filePath(), frame, direction);
    }
    return false;
}

bool FrmImage::lock(unsigned int fid)
{
    return lock(fid, 0, 0);
}

bool FrmImage::lock(unsigned int fid, int frame, int direction)
{
    if (isLocked()) {
        return false;
    }

    Art* art = artLock(fid, &_key);
    if (art == nullptr) {
        return false;
    }

    if (!setFrame(art, frame, direction)) {
        unlock();
        return false;
    }

    return true;
}

bool FrmImage::lock(const char* frmPath)
{
    return lock(frmPath, 0, 0);
}

bool FrmImage::lock(const char* frmPath, int frame, int direction)
{
    if (isLocked()) {
        return false;
    }

    _namedKey = artLockNamedFrameData(frmPath);
    if (!_namedKey) return false;

    if (!setFrame(_namedKey->art(), frame, direction)) {
        unlock();
        return false;
    }

    return true;
}

bool FrmImage::lock(ObjectType objType, const char* frmRelativePath)
{
    return lock(objType, frmRelativePath, 0, 0);
}

bool FrmImage::lock(ObjectType objType, const char* frmRelativePath, int frame, int direction)
{
    if (objType < OBJ_TYPE_ITEM || objType >= OBJ_TYPE_COUNT) {
        return false;
    }
    snprintf(_art_name, sizeof(_art_name), "%s%s%s\\%s", _cd_path_base, "art\\", gArtListDescriptions[objType].name, frmRelativePath);
    return lock(_art_name, frame, direction);
}

void FrmImage::unlock()
{
    if (_key != nullptr) {
        artUnlock(_key);
    }
    resetInternal();
}

void FrmImage::resetInternal()
{
    _namedKey = nullptr;
    _key = nullptr;
    _data = nullptr;
    _width = 0;
    _height = 0;
    _xOffset = 0;
    _yOffset = 0;
}

bool FrmImage::setFrame(const Art* art, int frame, int direction)
{
    unsigned char* data = artGetFrameData(art, frame, direction, &_width, &_height, &_xOffset, &_yOffset);
    if (data == nullptr) {
        return false;
    }

    _data = data;
    return true;
}

} // namespace fallout
