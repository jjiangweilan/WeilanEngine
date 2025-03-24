#pragma once
#include <memory>

class AutoGrowLinearAllocator
{
public:
    AutoGrowLinearAllocator(size_t initialSize = 1024);
    AutoGrowLinearAllocator(AutoGrowLinearAllocator&& other);
    ~AutoGrowLinearAllocator();
    AutoGrowLinearAllocator& operator=(AutoGrowLinearAllocator&& other);

    template <class T>
    T* Allocate(size_t count = 1)
    {
        size_t size = sizeof(T) * count;

        size_t targetSize = size + offset;
        GrowIfNeeded(targetSize);

        if (T* allocated = (T*)std::align(alignof(T), size, mem, memSize))
        {
            offset += size;
            return allocated;
        }

        return nullptr;
    }

    void Append(void* data, size_t size)
    {
        auto t = Allocate<unsigned char>(size);
        if (t) [[likely]]
        {
            memcpy(t, data, size);
        }
    }

    void Reset() { offset = 0; }

    void* GetMem() const { return mem; }
    size_t GetSize() const { return memSize; }

private:
    void GrowIfNeeded(size_t targetSize);
    void* mem = nullptr;
    size_t memSize = 0;
    size_t offset = 0;
};
