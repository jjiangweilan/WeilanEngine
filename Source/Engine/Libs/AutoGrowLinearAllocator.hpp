#pragma once
#include <memory>

class AutoGrowLinearAllocator
{
public:
    AutoGrowLinearAllocator(size_t initialSize = 1024);
    ~AutoGrowLinearAllocator();

    template <class T>
    T* Allocate(size_t count = 1)
    {
        size_t size = sizeof(T) * count;

        size_t targetSize = size + offset;
        GrowIfNeeded(targetSize);

        if (T* allocated = (T*)std::align(alignof(T), size, mem, memSize))
        {
            return allocated;
        }

        return nullptr;
    }

    void Reset() { offset = 0; }

private:
    void GrowIfNeeded(size_t targetSize);
    void* mem = nullptr;
    size_t memSize = 0;
    size_t offset = 0;
};
