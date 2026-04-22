#ifndef MEMORY_H
#define MEMORY_H

#include "memory_defs.h"

namespace fallout {

char* internal_strdup(const char* string);
void* internal_malloc(size_t size);
void* internal_realloc(void* ptr, size_t size);
void internal_free(void* ptr);
void mem_check();

// Owning smart pointer for objects allocated with internal_malloc.
template <typename T>
class InternalPtr {
public:
    InternalPtr() = default;
    explicit InternalPtr(T* ptr)
        : _ptr(ptr)
    {
    }
    ~InternalPtr() { reset(); }

    InternalPtr(const InternalPtr&) = delete;
    InternalPtr& operator=(const InternalPtr&) = delete;

    InternalPtr(InternalPtr&& other) noexcept
        : _ptr(other._ptr)
    {
        other._ptr = nullptr;
    }

    InternalPtr& operator=(InternalPtr&& other) noexcept
    {
        if (this != &other) {
            reset();
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        return *this;
    }

    void reset()
    {
        if (_ptr != nullptr) {
            internal_free(_ptr);
            _ptr = nullptr;
        }
    }

    T* get() const { return _ptr; }
    T* operator->() const { return _ptr; }
    T& operator*() const { return *_ptr; }
    explicit operator bool() const { return _ptr != nullptr; }

private:
    T* _ptr = nullptr;
};

} // namespace fallout

#endif /* MEMORY_H */
