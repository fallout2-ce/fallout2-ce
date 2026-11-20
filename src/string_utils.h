#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace fallout {

template <std::size_t Size>
void stringCopy(char (&dest)[Size], const char* src)
{
    static_assert(Size > 0);

    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }

    std::size_t length = std::min(std::strlen(src), Size - 1);
    std::memmove(dest, src, length);
    dest[length] = '\0';
}

template <std::size_t Size>
void stringAppend(char (&dest)[Size], const char* src)
{
    static_assert(Size > 0);

    if (src == nullptr) {
        return;
    }

    std::size_t destLength = std::strlen(dest);
    if (destLength >= Size - 1) {
        return;
    }

    std::size_t length = std::min(std::strlen(src), Size - destLength - 1);
    std::memmove(dest + destLength, src, length);
    dest[destLength + length] = '\0';
}

} // namespace fallout

#endif /* STRING_UTILS_H */
