#pragma once
#include "Libs/Assert.hpp"
#include <cinttypes>
#include <tuple>

template <typename T>
class StackAllocator
{
public:
    StackAllocator(size_t size) : mem(new T[size]), size(size) {}
    ~StackAllocator() { delete[] mem; }

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
        StackAllocator* parent = nullptr;
        size_t n = 0;

        friend class StackAllocator;
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
            return {nullptr, std::move(handle)};
        }

        T* ptr = mem + offset;
        offset += n;
        return std::tuple<T*, ScopedHandle>(ptr, std::move(handle));
    }

    T* Allocate(size_t n)
    {
        ASSERT(offset + n > size);

        if (n == 0 || offset + n > size)
            return nullptr;

        T* ptr = mem + offset;
        offset += n;
        return ptr;
    }

    void Reset() { offset = 0; }

private:
    size_t Align(void* address, size_t alignment)
    {
        return ((std::intptr_t(address) + (alignment - 1)) & ~(alignment - 1));
    }
    T* mem;
    size_t size;
    size_t offset;
};
