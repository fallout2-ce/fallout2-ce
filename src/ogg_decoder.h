#ifndef OGG_DECODER_H
#define OGG_DECODER_H

#include "audio.h"
#include "db.h"

namespace fallout {

bool oggDecoderIsFilePath(const char* filePath);
bool oggDecoderDecode(File* stream, AudioFileInfo* info, unsigned char** dataPtr, int* sizePtr);

} // namespace fallout

#endif /* OGG_DECODER_H */
