#ifndef FALLOUT_TOOLS_FO1_DAT_LZSS_H_
#define FALLOUT_TOOLS_FO1_DAT_LZSS_H_

#include <stdio.h>

namespace fallout {

int fo1LzssDecodeToBuf(FILE* in, unsigned char* dest, unsigned int length);

} // namespace fallout

#endif /* FALLOUT_TOOLS_FO1_DAT_LZSS_H_ */
