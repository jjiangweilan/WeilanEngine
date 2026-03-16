#pragma once
#include <cstdint>

class VirtualTLSFAllocator
{
public:
    struct Allocation
    {
        uint64_t offset = 0;
        uint64_t size = 0;
        void* internalHandle = nullptr;

        bool IsValid() const { return internalHandle != nullptr; }
    };

    VirtualTLSFAllocator(uint64_t totalSize);
    ~VirtualTLSFAllocator();

    VirtualTLSFAllocator(const VirtualTLSFAllocator&) = delete;
    VirtualTLSFAllocator& operator=(const VirtualTLSFAllocator&) = delete;

    VirtualTLSFAllocator(VirtualTLSFAllocator&& other) noexcept;
    VirtualTLSFAllocator& operator=(VirtualTLSFAllocator&& other) noexcept;

    bool Allocate(uint64_t size, uint32_t alignment, Allocation& outAllocation);
    void Free(Allocation& allocation);
    void Clear();

    uint64_t GetSize() const { return totalSize; }

private:
    void* block = nullptr; // Opaque VmaVirtualBlock handle
    uint64_t totalSize = 0;
};
