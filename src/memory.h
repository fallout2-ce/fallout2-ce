#ifndef MEMORY_H
#define MEMORY_H

#include <cstddef>
#include <type_traits>

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
    static_assert(std::is_trivially_destructible_v<T>);

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

    // Frees the resource and sets the pointer to null. Shouldn't normally be used, prefer destructor instead (RAII).
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
