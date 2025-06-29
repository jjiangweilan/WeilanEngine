#if _WIN64
#include "WindowsMemory.hpp"
#include <Windows.h>
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
        DWORD flags = tls ? HEAP_NO_SERIALIZE : 0;
        flags |= HEAP_ZERO_MEMORY;
        flags |= HEAP_GENERATE_EXCEPTIONS;
        void* p = HeapAlloc(processHeapHandle, flags, size);

        return p;
    }

    void* ReAllocateMemory(void* ptr, size_t newSize, bool tls)
    {
        if (ptr == nullptr)
        {
            return AllocateMemory(newSize, false, nullptr);
        }

        DWORD flags = tls ? HEAP_NO_SERIALIZE : 0;
        flags |= HEAP_ZERO_MEMORY;
        flags |= HEAP_GENERATE_EXCEPTIONS;
        flags |= HEAP_REALLOC_IN_PLACE_ONLY;
        void* newPtr = HeapReAlloc(processHeapHandle, flags, ptr, newSize);
        return newPtr;
    }

    void FreeMemory(void* ptr, bool tls)
    {
        if (ptr != nullptr)
        {

            DWORD flags = tls ? HEAP_NO_SERIALIZE : 0;
            HeapFree(processHeapHandle, flags, ptr);
        }
    }

private:
    PlatformMemoryManager() = default;

    HANDLE processHeapHandle = GetProcessHeap();
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
