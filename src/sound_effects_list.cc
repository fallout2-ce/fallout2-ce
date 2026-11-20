#include "sound_effects_list.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "debug.h"
#include "memory.h"
#include "platform_compat.h"
#include "sound_decoder.h"

namespace fallout {

typedef struct SoundEffectsListEntry {
    char* name;
    int dataSize;
    int fileSize;
    int tag;
} SoundEffectsListEntry;

static int soundEffectsListTagToIndex(int tag, int* indexPtr);
static void soundEffectsListClear();
static void soundEffectsListReset();
static int soundEffectsListPopulateFileNames();
static int soundEffectsListCopyFileNames(char** fileNameList);
static int soundEffectsListPopulateFileSizes();
static int soundEffectsListSort();
static int soundEffectsListCompareByName(const void* a, const void* b);
static int soundEffectsListSoundDecoderReadHandler(void* data, void* buf, unsigned int size);

// 0x51C8F8 sfxl_initialized
static bool gSoundEffectsListInitialized = false;

// 0x51C8FC sfxl_dlevel
static int gSoundEffectsListDebugLevel = INT_MAX;

// sfxl_effect_path
// 0x51C900 sfxl_effect_path
static char* gSoundEffectsListPath = nullptr;

// sfxl_effect_path_len
// 0x51C904 sfxl_effect_path_len
static int gSoundEffectsListPathLength = 0;

// sndlist.lst
//
// sfxl_list
// 0x51C908 sfxl_list
static SoundEffectsListEntry* gSoundEffectsListEntries = nullptr;

// The length of [gSoundEffectsListEntries] array.
//
// 0x51C90C sfxl_files_total
static int gSoundEffectsListEntriesLength = 0;

// 0x667F94 sfxl_compression
static int _sfxl_compression;

// sfxl_tag_is_legal
// 0x4A98E0 sfxl_tag_is_legal
bool soundEffectsListIsValidTag(int tag)
{
    return soundEffectsListTagToIndex(tag, nullptr) == SFXL_OK;
}

// sfxl_init
// 0x4A98F4 sfxl_init
int soundEffectsListInit(const char* soundEffectsPath, int compression, int debugLevel)
{
    gSoundEffectsListDebugLevel = debugLevel;
    _sfxl_compression = compression;
    gSoundEffectsListEntriesLength = 0;
    gSoundEffectsListPathLength = 0;

    gSoundEffectsListPath = internal_strdup(soundEffectsPath);
    if (gSoundEffectsListPath == nullptr) {
        return SFXL_ERR;
    }

    gSoundEffectsListPathLength = strlen(gSoundEffectsListPath);
    int err = soundEffectsListPopulateFileNames();
    if (err != SFXL_OK) {
        soundEffectsListReset();
        return err;
    }

    err = soundEffectsListPopulateFileSizes();
    if (err != SFXL_OK) {
        soundEffectsListReset();
        return err;
    }

    soundEffectsListSort();

    gSoundEffectsListInitialized = true;

    return SFXL_OK;
}

// 0x4A9C04 sfxl_exit
void soundEffectsListExit()
{
    if (gSoundEffectsListInitialized) {
        soundEffectsListReset();
    }
}

// sfxl_name_to_tag
// 0x4A9C28 sfxl_name_to_tag
int soundEffectsListGetTag(char* name, int* tagPtr)
{
    if (gSoundEffectsListPath == nullptr) {
        return SFXL_ERR;
    }

    if (compat_strnicmp(gSoundEffectsListPath, name, gSoundEffectsListPathLength) != 0) {
        return SFXL_ERR;
    }

    SoundEffectsListEntry dummy;
    dummy.name = name + gSoundEffectsListPathLength;

    SoundEffectsListEntry* entry = (SoundEffectsListEntry*)bsearch(&dummy, gSoundEffectsListEntries, gSoundEffectsListEntriesLength, sizeof(*gSoundEffectsListEntries), soundEffectsListCompareByName);
    if (entry == nullptr) {
        return SFXL_ERR;
    }

    int index = entry - gSoundEffectsListEntries;
    if (index < 0 || index >= gSoundEffectsListEntriesLength) {
        return SFXL_ERR;
    }

    *tagPtr = 2 * index + 2;

    return SFXL_OK;
}

// sfxl_name
// 0x4A9CD8 sfxl_name
int soundEffectsListGetFilePath(int tag, char** pathPtr)
{
    if (gSoundEffectsListPath == nullptr) {
        return SFXL_ERR;
    }

    int index;
    int err = soundEffectsListTagToIndex(tag, &index);
    if (err != SFXL_OK) {
        return err;
    }

    char* name = gSoundEffectsListEntries[index].name;

    char* path = (char*)internal_malloc(strlen(gSoundEffectsListPath) + strlen(name) + 1);
    if (path == nullptr) {
        return SFXL_ERR;
    }

    strcpy(path, gSoundEffectsListPath);
    strcat(path, name);

    *pathPtr = path;

    return SFXL_OK;
}

// 0x4A9D90 sfxl_size_full
int soundEffectsListGetDataSize(int tag, int* sizePtr)
{
    int index;
    int rc = soundEffectsListTagToIndex(tag, &index);
    if (rc != SFXL_OK) {
        return rc;
    }

    SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
    *sizePtr = entry->dataSize;

    return SFXL_OK;
}

// 0x4A9DBC sfxl_size_cached
int soundEffectsListGetFileSize(int tag, int* sizePtr)
{
    int index;
    int err = soundEffectsListTagToIndex(tag, &index);
    if (err != SFXL_OK) {
        return err;
    }

    SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
    *sizePtr = entry->fileSize;

    return SFXL_OK;
}

// sfxl_tag_to_index
// 0x4A9DE8 sfxl_index
static int soundEffectsListTagToIndex(int tag, int* indexPtr)
{
    if (tag <= 0) {
        return SFXL_ERR_TAG_INVALID;
    }

    if ((tag & 1) != 0) {
        return SFXL_ERR_TAG_INVALID;
    }

    int index = (tag / 2) - 1;
    if (index >= gSoundEffectsListEntriesLength) {
        return SFXL_ERR_TAG_INVALID;
    }

    if (indexPtr != nullptr) {
        *indexPtr = index;
    }

    return SFXL_OK;
}

// 0x4A9E44 sfxl_destroy
static void soundEffectsListClear()
{
    if (gSoundEffectsListEntriesLength < 0) {
        return;
    }

    if (gSoundEffectsListEntries == nullptr) {
        return;
    }

    for (int index = 0; index < gSoundEffectsListEntriesLength; index++) {
        SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
        if (entry->name != nullptr) {
            internal_free(entry->name);
        }
    }

    internal_free(gSoundEffectsListEntries);
    gSoundEffectsListEntries = nullptr;

    gSoundEffectsListEntriesLength = 0;
}

static void soundEffectsListReset()
{
    soundEffectsListClear();

    if (gSoundEffectsListPath != nullptr) {
        internal_free(gSoundEffectsListPath);
        gSoundEffectsListPath = nullptr;
    }

    gSoundEffectsListPathLength = 0;
    gSoundEffectsListEntriesLength = 0;
    gSoundEffectsListInitialized = false;
}

// sfxl_get_names
// 0x4A9EA0 sfxl_get_names
static int soundEffectsListPopulateFileNames()
{
    const char* extension;
    switch (_sfxl_compression) {
    case 0:
        extension = "*.SND";
        break;
    case 1:
        extension = "*.ACM";
        break;
    default:
        return SFXL_ERR;
    }

    char* pattern = (char*)internal_malloc(strlen(gSoundEffectsListPath) + strlen(extension) + 1);
    if (pattern == nullptr) {
        return SFXL_ERR;
    }

    strcpy(pattern, gSoundEffectsListPath);
    strcat(pattern, extension);

    char** fileNameList;
    gSoundEffectsListEntriesLength = fileNameListInit(pattern, &fileNameList);
    internal_free(pattern);

    if (gSoundEffectsListEntriesLength > 10000) {
        fileNameListFree(&fileNameList, 0);
        return SFXL_ERR;
    }

    if (gSoundEffectsListEntriesLength <= 0) {
        return SFXL_ERR;
    }

    gSoundEffectsListEntries = (SoundEffectsListEntry*)internal_malloc(sizeof(*gSoundEffectsListEntries) * gSoundEffectsListEntriesLength);
    if (gSoundEffectsListEntries == nullptr) {
        fileNameListFree(&fileNameList, 0);
        return SFXL_ERR;
    }

    memset(gSoundEffectsListEntries, 0, sizeof(*gSoundEffectsListEntries) * gSoundEffectsListEntriesLength);

    int err = soundEffectsListCopyFileNames(fileNameList);

    fileNameListFree(&fileNameList, 0);

    if (err != SFXL_OK) {
        soundEffectsListClear();
        return err;
    }

    return SFXL_OK;
}

// sfxl_copy_names
// 0x4AA000 sfxl_copy_names
static int soundEffectsListCopyFileNames(char** fileNameList)
{
    for (int index = 0; index < gSoundEffectsListEntriesLength; index++) {
        SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
        entry->name = internal_strdup(*fileNameList++);
        if (entry->name == nullptr) {
            soundEffectsListClear();
            return SFXL_ERR;
        }
    }

    return SFXL_OK;
}

// 0x4AA050 sfxl_get_sizes
static int soundEffectsListPopulateFileSizes()
{

    char* path = (char*)internal_malloc(gSoundEffectsListPathLength + COMPAT_MAX_PATH);
    if (path == nullptr) {
        return SFXL_ERR;
    }

    strcpy(path, gSoundEffectsListPath);

    char* fileName = path + gSoundEffectsListPathLength;

    for (int index = 0; index < gSoundEffectsListEntriesLength; index++) {
        SoundEffectsListEntry* entry = &(gSoundEffectsListEntries[index]);
        strcpy(fileName, entry->name);

        int fileSize;
        if (dbGetFileSize(path, &fileSize) != 0) {
            internal_free(path);
            return SFXL_ERR;
        }

        if (fileSize <= 0) {
            internal_free(path);
            return SFXL_ERR;
        }

        entry->fileSize = fileSize;

        switch (_sfxl_compression) {
        case 0:
            entry->dataSize = fileSize;
            break;
        case 1:
            if (1) {
                File* stream = fileOpen(path, "rb");
                if (stream == nullptr) {
                    internal_free(path);
                    return 1;
                }

                int channels;
                int sampleRate;
                int sampleCount;
                SoundDecoder* soundDecoder = soundDecoderInit(soundEffectsListSoundDecoderReadHandler, stream, &channels, &sampleRate, &sampleCount);
                if (soundDecoder == nullptr) {
                    debugPrint("SFXLIST: Failed to decode ACM header for %s\n", path);
                    fileClose(stream);
                    internal_free(path);
                    return SFXL_ERR;
                }

                entry->dataSize = 2 * sampleCount;
                soundDecoderFree(soundDecoder);
                fileClose(stream);
            }
            break;
        default:
            internal_free(path);
            return SFXL_ERR;
        }
    }

    internal_free(path);

    return SFXL_OK;
}

// NOTE: Inlined.
//
// 0x4AA200 sfxl_sort_by_name
static int soundEffectsListSort()
{
    if (gSoundEffectsListEntriesLength != 1) {
        qsort(gSoundEffectsListEntries, gSoundEffectsListEntriesLength, sizeof(*gSoundEffectsListEntries), soundEffectsListCompareByName);
    }
    return 0;
}

// 0x4AA228 sfxl_compare_by_name
static int soundEffectsListCompareByName(const void* a, const void* b)
{
    SoundEffectsListEntry* lhs = (SoundEffectsListEntry*)a;
    SoundEffectsListEntry* rhs = (SoundEffectsListEntry*)b;

    return compat_stricmp(lhs->name, rhs->name);
}

// read via xfile
static int soundEffectsListSoundDecoderReadHandler(void* data, void* buf, unsigned int size)
{
    return fileRead(buf, 1, size, reinterpret_cast<File*>(data));
}

} // namespace fallout
