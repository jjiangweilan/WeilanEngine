#if __APPLE__
#include "MacOSMemory.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

class PlatformMemoryManager
{
public:
    static PlatformMemoryManager& Instance()
    {
        static PlatformMemoryManager instance;
        return instance;
    }

    void* AllocateMemory(size_t size, bool tls, const char* allocationTag)
    {
        // Use aligned_alloc for better alignment guarantees
        // On macOS, malloc already provides alignment suitable for any standard type
        void* ptr = std::malloc(size);
        
        if (ptr != nullptr)
        {
            // Zero-initialize the memory similar to Windows HEAP_ZERO_MEMORY
            std::memset(ptr, 0, size);
        }
        
        return ptr;
    }

    void* ReAllocateMemory(void* ptr, size_t newSize, bool tls)
    {
        if (ptr == nullptr)
        {
            return AllocateMemory(newSize, false, nullptr);
        }

        void* oldPtr = ptr;
        void* newPtr = std::realloc(ptr, newSize);
        
        // Handle allocation failure
        if (newPtr == nullptr && newSize > 0)
        {
            throw std::bad_alloc();
        }
        
        // Handle address space change
        if (newPtr != oldPtr)
        {
            throw std::runtime_error("Memory reallocation changed address space");
        }
        
        // Note: realloc doesn't zero-initialize new memory like HeapReAlloc with HEAP_ZERO_MEMORY
        // If you need this behavior, you'd need to track old size and zero the new portion
        
        return newPtr;
    }

    void FreeMemory(void* ptr, bool tls)
    {
        if (ptr != nullptr)
        {
            std::free(ptr);
        }
    }

private:
    PlatformMemoryManager() = default;
};

void* Platform_AllocateMemory(size_t size, bool tls, const char* allocationTag)
{
    return PlatformMemoryManager::Instance().AllocateMemory(size, tls, allocationTag);
}

void Platform_FreeMemory(void* ptr, bool tls)
{
    PlatformMemoryManager::Instance().FreeMemory(ptr, tls);
}

void* Platform_ReAllocateMemory(void* ptr, size_t newSize, bool tls)
{
    return PlatformMemoryManager::Instance().ReAllocateMemory(ptr, newSize, tls);
}

#endif
