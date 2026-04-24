// NOTE: For unknown reason functions in this module use __stdcall instead
// of regular __usercall.

#include "file_utils.h"

#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include <vector>

#include "platform_compat.h"

namespace fallout {

static void fileCopy(const char* existingFilePath, const char* newFilePath);
static bool copyGzToFile(gzFile inStream, FILE* outStream);
static bool copyFileToGz(FILE* inStream, gzFile outStream);

// 0x452740
int fileCopyDecompressed(const char* existingFilePath, const char* newFilePath)
{
    FILE* stream = compat_fopen(existingFilePath, "rb");
    if (stream == nullptr) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(stream);
    magic[1] = fgetc(stream);
    fclose(stream);

    if (magic[0] == 0x1F && magic[1] == 0x8B) {
        gzFile inStream = compat_gzopen(existingFilePath, "rb");
        FILE* outStream = compat_fopen(newFilePath, "wb");

        if (inStream != nullptr && outStream != nullptr) {
            if (!copyGzToFile(inStream, outStream)) {
                gzclose(inStream);
                fclose(outStream);
                return -1;
            }

            gzclose(inStream);
            fclose(outStream);
        } else {
            if (inStream != nullptr) {
                gzclose(inStream);
            }

            if (outStream != nullptr) {
                fclose(outStream);
            }

            return -1;
        }
    } else {
        fileCopy(existingFilePath, newFilePath);
    }

    return 0;
}

// 0x452804
int fileCopyCompressed(const char* existingFilePath, const char* newFilePath)
{
    FILE* inStream = compat_fopen(existingFilePath, "rb");
    if (inStream == nullptr) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(inStream);
    magic[1] = fgetc(inStream);
    rewind(inStream);

    if (magic[0] == 0x1F && magic[1] == 0x8B) {
        // Source file is already gzipped, there is no need to do anything
        // besides copying.
        fclose(inStream);
        fileCopy(existingFilePath, newFilePath);
    } else {
        gzFile outStream = compat_gzopen(newFilePath, "wb");
        if (outStream == nullptr) {
            fclose(inStream);
            return -1;
        }

        if (!copyFileToGz(inStream, outStream)) {
            fclose(inStream);
            gzclose(outStream);
            return -1;
        }

        fclose(inStream);
        gzclose(outStream);
    }

    return 0;
}

// TODO: Check, implementation looks odd.
int _gzdecompress_file(const char* existingFilePath, const char* newFilePath)
{
    FILE* stream = compat_fopen(existingFilePath, "rb");
    if (stream == nullptr) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(stream);
    magic[1] = fgetc(stream);
    fclose(stream);

    // TODO: Is it broken?
    if (magic[0] != 0x1F || magic[1] != 0x8B) {
        gzFile gzstream = compat_gzopen(existingFilePath, "rb");
        if (gzstream == nullptr) {
            return -1;
        }

        stream = compat_fopen(newFilePath, "wb");
        if (stream == nullptr) {
            gzclose(gzstream);
            return -1;
        }

        if (!copyGzToFile(gzstream, stream)) {
            gzclose(gzstream);
            fclose(stream);
            return -1;
        }

        gzclose(gzstream);
        fclose(stream);
    } else {
        fileCopy(existingFilePath, newFilePath);
    }

    return 0;
}

static void fileCopy(const char* existingFilePath, const char* newFilePath)
{
    FILE* in = compat_fopen(existingFilePath, "rb");
    FILE* out = compat_fopen(newFilePath, "wb");
    if (in != nullptr && out != nullptr) {
        std::vector<unsigned char> buffer(0xFFFF);

        size_t bytesRead;
        while ((bytesRead = fread(buffer.data(), sizeof(*buffer.data()), buffer.size(), in)) > 0) {
            size_t bytesWritten;
            size_t offset = 0;
            while ((bytesWritten = fwrite(buffer.data() + offset, sizeof(*buffer.data()), bytesRead, out)) > 0) {
                bytesRead -= bytesWritten;
                offset += bytesWritten;
            }

            if (bytesWritten < 0) {
                bytesRead = -1;
                break;
            }
        }
    }

    if (in != nullptr) {
        fclose(in);
    }

    if (out != nullptr) {
        fclose(out);
    }
}

static bool copyGzToFile(gzFile inStream, FILE* outStream)
{
    std::vector<unsigned char> buffer(0xFFFF);

    for (;;) {
        int bytesRead = gzread(inStream, buffer.data(), buffer.size());
        if (bytesRead < 0) {
            return false;
        }

        if (bytesRead == 0) {
            return true;
        }

        size_t offset = 0;
        size_t remaining = bytesRead;
        while (remaining > 0) {
            size_t bytesWritten = fwrite(buffer.data() + offset, sizeof(*buffer.data()), remaining, outStream);
            if (bytesWritten == 0) {
                return false;
            }

            offset += bytesWritten;
            remaining -= bytesWritten;
        }
    }
}

static bool copyFileToGz(FILE* inStream, gzFile outStream)
{
    std::vector<unsigned char> buffer(0xFFFF);

    for (;;) {
        size_t bytesRead = fread(buffer.data(), sizeof(*buffer.data()), buffer.size(), inStream);
        if (bytesRead == 0) {
            return feof(inStream) != 0;
        }

        size_t offset = 0;
        size_t remaining = bytesRead;
        while (remaining > 0) {
            int bytesWritten = gzwrite(outStream, buffer.data() + offset, remaining);
            if (bytesWritten <= 0) {
                return false;
            }

            offset += bytesWritten;
            remaining -= bytesWritten;
        }
    }
}

} // namespace fallout
