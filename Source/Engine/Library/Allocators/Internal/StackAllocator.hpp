#pragma once
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Library/Platforms/Memory.hpp"
#include <tuple>

namespace LowLevelAllocators
{
template <bool lts = true>
class StackAllocator
{
public:
    StackAllocator(size_t size) : mem(nullptr), size(size), offset(0)
    {
        mem = (unsigned char*)Platform_AllocateMemory(size, lts, "LowLevel - StackAllocator");
    }

    ~StackAllocator() { Platform_FreeMemory(mem, lts); }

    class ScopedHandle
    {
    public:
        ScopedHandle() {}
        ScopedHandle(const ScopedHandle&) = delete;
        ScopedHandle(ScopedHandle&& other) noexcept
            : parent(other.parent), n(other.n), ptr(other.ptr), alignment(other.alignment)
        {
            other.parent = nullptr;
            other.n = 0;
            other.ptr = nullptr;
        }
        ~ScopedHandle()
        {
            if (parent != nullptr)
                parent->Deallocate(ptr, n, alignment);
        }

    private:
        void* ptr = nullptr; // pointer to the allocated memory
        StackAllocator* parent = nullptr;
        size_t n = 0;
        size_t alignment = 0;

        friend class StackAllocator;
    };

    void* Allocate(size_t bytes, size_t alignment)
    {
        ASSERT(alignment == 0 || Math::IsPowerOfTwo(alignment)); // Alignment must be a power of two
        if (bytes == 0)
            return nullptr;

        size_t roundUpBytes = Math::RoundToAlignmentPoT(bytes, alignment);
        size_t targetPtr = this->offset + roundUpBytes;

        if (targetPtr > this->size)
        {
            GrowAllocation(targetPtr);
        }

        size_t padding = roundUpBytes - bytes;

        void* ptr = mem + padding;
        this->offset = targetPtr;
        return ptr;
    }

    void Deallocate(void* ptr, size_t bytes, size_t alignment)
    {
        ASSERT(alignment == 0 || Math::IsPowerOfTwo(alignment)); // Alignment must be a power of two
        if (bytes == 0)
            return;

        if ((unsigned char*)ptr + bytes == mem + offset)
        {
            size_t roundUpBytes = Math::RoundToAlignmentPoT(bytes, alignment);
            this->offset -= roundUpBytes;
        }
    }

    void Reset() { offset = 0; }

    /*========================= Public Utilities =========================*/

    // Allocate n elements of type T.
    // It's the callee's responsibility to ensure that the handle is deallocated in a linearly way
    // DO NOT store the handle
    template <class T>
    std::tuple<T*, ScopedHandle> ScopedAllocate(size_t n, size_t alignment = alignof(T))
    {
        ScopedHandle handle{};
        handle.parent = this;
        handle.n = n;
        handle.alignment = alignment;

        T* allocated = (T*)Allocate(n, alignment);

        if (allocated == nullptr)
        {
            handle.parent = nullptr;
            return {nullptr, std::move(handle)};
        }

        handle.ptr = allocated;
        return std::tuple<T*, ScopedHandle>(allocated, std::move(handle));
    }

private:
    void GrowAllocation(size_t leastSize)
    {
        size_t newSize = size * 2; // double the size
        while (newSize < leastSize)
        {
            newSize *= 2;
        }
        unsigned char* newMem = static_cast<unsigned char*>(Platform_ReAllocateMemory(mem, newSize, lts));
        mem = newMem;
        size = newSize;
    }

    unsigned char* mem;
    size_t offset = 0;
    size_t size = 0;
};
} // namespace LowLevelAllocators
