#ifndef AUDIO_H
#define AUDIO_H

namespace fallout {

typedef bool(AudioQueryCompressedFunc)(char* filePath);

typedef struct AudioFileInfo {
    int channels;
    int sampleRate;
    int bitsPerSample;
} AudioFileInfo;

int audioOpen(const char* fname, int* sampleRate);
int audioClose(int handle);
int audioRead(int handle, void* buffer, unsigned int size);
long audioSeek(int handle, long offset, int origin);
long audioGetSize(int handle);
long audioTell(int handle);
int audioWrite(int handle, const void* buf, unsigned int size);
bool audioProbeFile(const char* fname, AudioFileInfo* info);
int audioInit(AudioQueryCompressedFunc* func);
void audioExit();

} // namespace fallout

#endif /* AUDIO_H */
