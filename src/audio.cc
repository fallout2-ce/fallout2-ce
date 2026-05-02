#include "audio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "db.h"
#include "debug.h"
#include "memory_manager.h"
#include "ogg_decoder.h"
#include "platform_compat.h"
#include "sound.h"
#include "sound_decoder.h"

namespace fallout {

typedef enum AudioFlags {
    AUDIO_IN_USE = 0x01,
    AUDIO_COMPRESSED = 0x02,
    AUDIO_MEMORY = 0x04,
} AudioFileFlags;

typedef struct Audio {
    int flags;
    File* stream;
    SoundDecoder* soundDecoder;
    unsigned char* data;
    int fileSize;
    int sampleRate;
    int channels;
    int bitsPerSample;
    int position;
} Audio;

typedef enum AudioOpenMode {
    AUDIO_OPEN_MODE_RAW = 0,
    AUDIO_OPEN_MODE_COMPRESSED = 1,
    AUDIO_OPEN_MODE_WAV = 2,
    AUDIO_OPEN_MODE_OGG = 3,
} AudioOpenMode;

static bool defaultCompressionFunc(char* filePath);
static int audioSoundDecoderReadHandler(void* data, void* buf, unsigned int size);
static bool audioIsWavePath(const char* filePath);
static bool audioDecodeWave(File* stream, AudioFileInfo* info, unsigned char** dataPtr, int* sizePtr);

// 0x5108BC
static AudioQueryCompressedFunc* queryCompressedFunc = defaultCompressionFunc;

// 0x56CB00
static int gAudioListLength;

// 0x56CB04
static Audio* gAudioList;

// 0x41A2B0
static bool defaultCompressionFunc(char* filePath)
{
    char* pch = strrchr(filePath, '.');
    if (pch != nullptr) {
        strcpy(pch + 1, "raw");
    }

    return false;
}

// 0x41A2D0
static int audioSoundDecoderReadHandler(void* data, void* buffer, unsigned int size)
{
    return fileRead(buffer, 1, size, reinterpret_cast<File*>(data));
}

static bool audioIsWavePath(const char* filePath)
{
    const char* dot = strrchr(filePath, '.');
    return dot != nullptr && compat_stricmp(dot + 1, "wav") == 0;
}

static bool audioDecodeWave(File* stream, AudioFileInfo* info, unsigned char** dataPtr, int* sizePtr)
{
    bool success = false;
    unsigned char* fileData = nullptr;
    Uint8* loadedData = nullptr;
    Uint8* convertedData = nullptr;
    SDL_RWops* rw = nullptr;
    SDL_AudioSpec spec = {};
    Uint32 loadedLength = 0;
    int channels = 0;
    SDL_AudioCVT cvt;
    memset(&cvt, 0, sizeof(cvt));
    int convertedLength = 0;
    bool convertedDataNeedsFree = false;
    bool loadedDataNeedsFree = false;

    int fileSize = fileGetSize(stream);
    if (fileSize <= 0) {
        return false;
    }

    fileData = reinterpret_cast<unsigned char*>(internal_malloc_safe(fileSize, __FILE__, __LINE__));
    if (fileRead(fileData, 1, fileSize, stream) != fileSize) {
        goto done;
    }

    rw = SDL_RWFromConstMem(fileData, fileSize);
    if (rw == nullptr) {
        goto done;
    }

    if (SDL_LoadWAV_RW(rw, 1, &spec, &loadedData, &loadedLength) == nullptr) {
        rw = nullptr;
        goto done;
    }
    rw = nullptr;
    loadedDataNeedsFree = true;

    channels = spec.channels == 1 ? 1 : 2;
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, channels, spec.freq) < 0) {
        goto done;
    }

    convertedLength = static_cast<int>(loadedLength);
    if (cvt.needed != 0) {
        cvt.len = static_cast<int>(loadedLength);
        cvt.buf = reinterpret_cast<Uint8*>(SDL_malloc(cvt.len * cvt.len_mult));
        if (cvt.buf == nullptr) {
            goto done;
        }

        memcpy(cvt.buf, loadedData, loadedLength);
        if (SDL_ConvertAudio(&cvt) != 0) {
            SDL_free(cvt.buf);
            goto done;
        }

        convertedData = cvt.buf;
        convertedLength = cvt.len_cvt;
        convertedDataNeedsFree = true;
    } else {
        convertedData = loadedData;
        loadedDataNeedsFree = false;
    }

    if (info != nullptr) {
        info->channels = channels;
        info->sampleRate = spec.freq;
        info->bitsPerSample = 16;
    }

    if (dataPtr != nullptr && sizePtr != nullptr) {
        *dataPtr = reinterpret_cast<unsigned char*>(internal_malloc_safe(convertedLength, __FILE__, __LINE__));
        memcpy(*dataPtr, convertedData, convertedLength);
        *sizePtr = convertedLength;
    }

    success = true;

done:
    if (rw != nullptr) {
        SDL_RWclose(rw);
    }

    if (convertedData != nullptr) {
        if (convertedDataNeedsFree) {
            SDL_free(convertedData);
        }
    }

    if (loadedDataNeedsFree && loadedData != nullptr) {
        SDL_FreeWAV(loadedData);
    }

    if (fileData != nullptr) {
        internal_free_safe(fileData, __FILE__, __LINE__);
    }

    return success;
}

// AudioOpen
// 0x41A2EC
int audioOpen(const char* fname, int* sampleRate)
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s", fname);

    AudioOpenMode openMode;
    if (audioIsWavePath(path)) {
        openMode = AUDIO_OPEN_MODE_WAV;
    } else if (oggDecoderIsFilePath(path)) {
        openMode = AUDIO_OPEN_MODE_OGG;
    } else if (queryCompressedFunc(path)) {
        openMode = AUDIO_OPEN_MODE_COMPRESSED;
    } else {
        openMode = AUDIO_OPEN_MODE_RAW;
    }

    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        debugPrint("AudioOpen: Couldn't open %s for read\n", path);
        return -1;
    }

    int index;
    for (index = 0; index < gAudioListLength; index++) {
        if ((gAudioList[index].flags & AUDIO_IN_USE) == 0) {
            break;
        }
    }

    if (index == gAudioListLength) {
        if (gAudioList != nullptr) {
            gAudioList = (Audio*)internal_realloc_safe(gAudioList, sizeof(*gAudioList) * (gAudioListLength + 1), __FILE__, __LINE__); // "..\int\audio.c", 216
        } else {
            gAudioList = (Audio*)internal_malloc_safe(sizeof(*gAudioList), __FILE__, __LINE__); // "..\int\audio.c", 218
        }
        gAudioListLength++;
    }

    Audio* audioFile = &(gAudioList[index]);
    memset(audioFile, 0, sizeof(*audioFile));
    audioFile->flags = AUDIO_IN_USE;
    audioFile->stream = stream;

    if (openMode == AUDIO_OPEN_MODE_WAV || openMode == AUDIO_OPEN_MODE_OGG) {
        AudioFileInfo info;
        audioFile->flags |= AUDIO_MEMORY;
        bool decoded = false;
        if (openMode == AUDIO_OPEN_MODE_WAV) {
            decoded = audioDecodeWave(stream, &info, &(audioFile->data), &(audioFile->fileSize));
        } else {
            decoded = oggDecoderDecode(stream, &info, &(audioFile->data), &(audioFile->fileSize));
        }

        if (!decoded) {
            fileClose(stream);
            memset(audioFile, 0, sizeof(*audioFile));
            debugPrint("AudioOpen: Couldn't decode %s\n", path);
            return -1;
        }

        fileClose(stream);
        audioFile->stream = nullptr;
        audioFile->sampleRate = info.sampleRate;
        audioFile->channels = info.channels;
        audioFile->bitsPerSample = info.bitsPerSample;
        *sampleRate = audioFile->sampleRate;
    } else if (openMode == AUDIO_OPEN_MODE_COMPRESSED) {
        audioFile->flags |= AUDIO_COMPRESSED;
        audioFile->soundDecoder = soundDecoderInit(audioSoundDecoderReadHandler, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
        audioFile->fileSize *= 2;
        audioFile->bitsPerSample = 16;

        *sampleRate = audioFile->sampleRate;
    } else {
        audioFile->fileSize = fileGetSize(stream);
        audioFile->bitsPerSample = 8;
    }

    audioFile->position = 0;

    return index + 1;
}

// 0x41A50C
int audioClose(int handle)
{
    Audio* audioFile = &(gAudioList[handle - 1]);
    if ((audioFile->flags & AUDIO_MEMORY) != 0) {
        if (audioFile->data != nullptr) {
            internal_free_safe(audioFile->data, __FILE__, __LINE__);
        }
    } else if (audioFile->stream != nullptr) {
        fileClose(audioFile->stream);
    }

    if ((audioFile->flags & AUDIO_COMPRESSED) != 0) {
        soundDecoderFree(audioFile->soundDecoder);
    }

    memset(audioFile, 0, sizeof(Audio));

    return 0;
}

// 0x41A574
int audioRead(int handle, void* buffer, unsigned int size)
{
    Audio* audioFile = &(gAudioList[handle - 1]);

    int bytesRead;
    if ((audioFile->flags & AUDIO_MEMORY) != 0) {
        bytesRead = audioFile->fileSize - audioFile->position;
        if (bytesRead > static_cast<int>(size)) {
            bytesRead = size;
        }

        if (bytesRead > 0) {
            memcpy(buffer, audioFile->data + audioFile->position, bytesRead);
        }
    } else if ((audioFile->flags & AUDIO_COMPRESSED) != 0) {
        bytesRead = soundDecoderDecode(audioFile->soundDecoder, buffer, size);
    } else {
        bytesRead = fileRead(buffer, 1, size, audioFile->stream);
    }

    audioFile->position += bytesRead;

    return bytesRead;
}

// 0x41A5E0
long audioSeek(int handle, long offset, int origin)
{
    int pos;
    unsigned char* buf;
    int remainingBytesToSkip;

    Audio* audioFile = &(gAudioList[handle - 1]);

    switch (origin) {
    case SEEK_SET:
        pos = offset;
        break;
    case SEEK_CUR:
        pos = offset + audioFile->position;
        break;
    case SEEK_END:
        pos = offset + audioFile->fileSize;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    if ((audioFile->flags & AUDIO_MEMORY) != 0) {
        if (pos < 0) {
            pos = 0;
        }

        if (pos > audioFile->fileSize) {
            pos = audioFile->fileSize;
        }

        audioFile->position = pos;
        return audioFile->position;
    } else if ((audioFile->flags & AUDIO_COMPRESSED) != 0) {
        if (pos < audioFile->position) {
            soundDecoderFree(audioFile->soundDecoder);
            fileSeek(audioFile->stream, 0, SEEK_SET);
            audioFile->soundDecoder = soundDecoderInit(audioSoundDecoderReadHandler, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
            audioFile->position = 0;
            audioFile->fileSize *= 2;

            if (pos != 0) {
                buf = (unsigned char*)internal_malloc_safe(4096, __FILE__, __LINE__); // "..\int\audio.c", 361
                while (pos > 4096) {
                    pos -= 4096;
                    audioRead(handle, buf, 4096);
                }

                if (pos != 0) {
                    audioRead(handle, buf, pos);
                }

                internal_free_safe(buf, __FILE__, __LINE__); // // "..\int\audio.c", 367
            }
        } else {
            buf = (unsigned char*)internal_malloc_safe(1024, __FILE__, __LINE__); // "..\int\audio.c", 321
            remainingBytesToSkip = audioFile->position - pos;
            while (remainingBytesToSkip > 1024) {
                remainingBytesToSkip -= 1024;
                audioRead(handle, buf, 1024);
            }

            if (remainingBytesToSkip != 0) {
                audioRead(handle, buf, remainingBytesToSkip);
            }

            // TODO: Probably leaks memory.
        }

        return audioFile->position;
    } else {
        return fileSeek(audioFile->stream, offset, origin);
    }
}

// 0x41A78C
long audioGetSize(int handle)
{
    Audio* audioFile = &(gAudioList[handle - 1]);
    return audioFile->fileSize;
}

// 0x41A7A8
long audioTell(int handle)
{
    Audio* audioFile = &(gAudioList[handle - 1]);
    return audioFile->position;
}

// AudioWrite
// 0x41A7C4
int audioWrite(int handle, const void* buf, unsigned int size)
{
    debugPrint("AudioWrite shouldn't be ever called\n");
    return 0;
}

bool audioProbeFile(const char* fname, AudioFileInfo* info)
{
    if (info == nullptr) {
        return false;
    }

    memset(info, 0, sizeof(*info));

    if (!audioIsWavePath(fname) && !oggDecoderIsFilePath(fname)) {
        return false;
    }

    File* stream = fileOpen(fname, "rb");
    if (stream == nullptr) {
        return false;
    }

    bool success;
    if (audioIsWavePath(fname)) {
        success = audioDecodeWave(stream, info, nullptr, nullptr);
    } else {
        success = oggDecoderDecode(stream, info, nullptr, nullptr);
    }
    fileClose(stream);

    return success;
}

// 0x41A7D4
int audioInit(AudioQueryCompressedFunc* func)
{
    queryCompressedFunc = func;
    gAudioList = nullptr;
    gAudioListLength = 0;

    return soundSetDefaultFileIO(audioOpen, audioClose, audioRead, audioWrite, audioSeek, audioTell, audioGetSize);
}

// 0x41A818
void audioExit()
{
    if (gAudioList != nullptr) {
        internal_free_safe(gAudioList, __FILE__, __LINE__); // "..\int\audio.c", 406
    }

    gAudioListLength = 0;
    gAudioList = nullptr;
}

} // namespace fallout
