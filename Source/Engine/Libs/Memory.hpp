#pragma once
#include "Allocators/StackAllocator.hpp"
#include <memory_resource>

// fast, thread local memory allocation
// should be used for small allocations within a function scope
using StackAllocator = LowLevelAllocators::StackAllocator<true>;

// using shared memory within a frame
// ideally to be used for temporary allocation that need to communicate within a frame
// reset at the end of the frame
class FrameMemoryAllocator
{};

// slow, but not restricted in size. Manual deallocation required
// designed to have low fragmentation
class PermenentMemoryAllocator
{};

StackAllocator& GetStackAllocator();
StackAllocator& GetSharedStackAllocator();

class LocalStackMemoryAllocator : public std::pmr::memory_resource
{
public:
    LocalStackMemoryAllocator() = default;
    ~LocalStackMemoryAllocator() override { GetStackAllocator().Reset(); }

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        return GetStackAllocator().Allocate(bytes, alignment);
    }

    // void do_deallocate(T* p, std::size_t n) { GetStackAllocator().Deallocate((unsigned char*)p, n * sizeof(T)); }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
    {
        return GetStackAllocator().Deallocate(p, bytes, alignment);
    }

    // all LocalStackMemoryAllocator instances are wrapper of the global GetStackAllocator()
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return true; }
};

template <class T>
class GlobalMemoryAllocator : public std::allocator<T>
{
public:
    constexpr T* allocate(std::size_t n) { return reinterpret_cast<T*>(::operator new(sizeof(T) * n)); }
    constexpr void deallocate(T* p, std::size_t n) { ::operator delete(p); }
};
