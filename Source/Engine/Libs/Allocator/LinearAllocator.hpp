#pragma once
#include "Libs/Assert.hpp"
#include <cinttypes>
#include <tuple>

template <typename T>
class LinearAllocator
{
public:
    LinearAllocator(size_t size) : mem(new T[size]), size(size) {}
    ~LinearAllocator() { delete[] mem; }

    class ScopedHandle
    {
    public:
        ScopedHandle() {}
        ScopedHandle(const ScopedHandle&) = delete;
        ScopedHandle(ScopedHandle&& other) noexcept : parent(other.parent), n(other.n)
        {
            other.parent = nullptr;
            other.n = 0;
        }
        ~ScopedHandle()
        {
            if (parent != nullptr)
            {
                parent->offset -= n;
            }
        }

    private:
        LinearAllocator* parent = nullptr;
        size_t n = 0;

        friend class LinearAllocator;
    };

    // Allocate n elements of type T.
    // It's the callee's responsibility to ensure that the handle is deallocated in a linearly way
    // DO NOT store the handle
    std::tuple<T*, ScopedHandle> ScopedAllocate(size_t n)
    {
        ScopedHandle handle{};
        handle.parent = this;
        handle.n = n;

        if (n == 0)
        {
            handle.parent = nullptr;
            return { nullptr, std::move(handle) };
        }

        T* ptr = mem + offset;
        offset += n;
        return std::tuple<T*, ScopedHandle>(ptr, std::move(handle));
    }

    T* Allocate(size_t n, const char* tag)
    {
        ASSERT(offset + n < size);

        if (n == 0 || offset + n > size)
            return nullptr;

        T* ptr = mem + offset;
        offset += n;
        return ptr;
    }

    void Reset() { offset = 0; }

private:
    T* mem;
    size_t size;
    size_t offset;
};
