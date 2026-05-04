#include "ogg_decoder.h"

#include <stdlib.h>
#include <string.h>

#include "memory_manager.h"
#include "platform_compat.h"

#if HAVE_STB_VORBIS
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"
#endif

namespace fallout {

bool oggDecoderIsFilePath(const char* filePath)
{
    const char* dot = strrchr(filePath, '.');
    return dot != nullptr && compat_stricmp(dot + 1, "ogg") == 0;
}

#if HAVE_STB_VORBIS

bool oggDecoderDecode(File* stream, AudioFileInfo* info, unsigned char** dataPtr, int* sizePtr)
{
    bool success = false;
    unsigned char* compressedData = nullptr;
    short* decodedData = nullptr;
    stb_vorbis* vorbis = nullptr;
    int error = 0;
    stb_vorbis_info vorbisInfo = {};
    int totalSamples = 0;
    int sampleCount = 0;
    int samplesDecoded = 0;

    int fileSize = fileGetSize(stream);
    if (fileSize <= 0) {
        return false;
    }

    compressedData = reinterpret_cast<unsigned char*>(internal_malloc_safe(fileSize, __FILE__, __LINE__));
    if (fileRead(compressedData, 1, fileSize, stream) != fileSize) {
        goto done;
    }

    vorbis = stb_vorbis_open_memory(compressedData, fileSize, &error, nullptr);
    if (vorbis == nullptr) {
        goto done;
    }

    vorbisInfo = stb_vorbis_get_info(vorbis);
    if (vorbisInfo.channels < 1 || vorbisInfo.channels > 2) {
        goto done;
    }

    totalSamples = stb_vorbis_stream_length_in_samples(vorbis);
    if (totalSamples <= 0) {
        goto done;
    }

    sampleCount = totalSamples * vorbisInfo.channels;
    decodedData = reinterpret_cast<short*>(internal_malloc_safe(sizeof(short) * sampleCount, __FILE__, __LINE__));

    while (samplesDecoded < totalSamples) {
        int chunkSamples = stb_vorbis_get_samples_short_interleaved(vorbis,
            vorbisInfo.channels,
            decodedData + samplesDecoded * vorbisInfo.channels,
            sampleCount - samplesDecoded * vorbisInfo.channels);
        if (chunkSamples <= 0) {
            break;
        }

        samplesDecoded += chunkSamples;
    }

    if (samplesDecoded <= 0) {
        goto done;
    }

    if (info != nullptr) {
        info->channels = vorbisInfo.channels;
        info->sampleRate = vorbisInfo.sample_rate;
        info->bitsPerSample = 16;
    }

    if (dataPtr != nullptr && sizePtr != nullptr) {
        *sizePtr = samplesDecoded * vorbisInfo.channels * sizeof(short);
        *dataPtr = reinterpret_cast<unsigned char*>(decodedData);
        decodedData = nullptr;
    }

    success = true;

done:
    if (vorbis != nullptr) {
        stb_vorbis_close(vorbis);
    }

    if (decodedData != nullptr) {
        internal_free_safe(decodedData, __FILE__, __LINE__);
    }

    if (compressedData != nullptr) {
        internal_free_safe(compressedData, __FILE__, __LINE__);
    }

    return success;
}

#else

bool oggDecoderDecode(File* stream, AudioFileInfo* info, unsigned char** dataPtr, int* sizePtr)
{
    return false;
}

#endif

} // namespace fallout
