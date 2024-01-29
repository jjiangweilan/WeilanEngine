#pragma once
#include <memory>
#include <vector>

template <size_t chunkSize>
class LinearAllocator
{
    struct Chunk
    {
        size_t space;
        void* p; // next allocate pointer
        uint8_t* data;
    };

public:
    LinearAllocator()
    {
        AddChunk();
        freeChunkIndex = 0;
    };

    ~LinearAllocator()
    {
        for (auto& c : chunks)
        {
            delete[] c.data;
        }
    }

    template <class T>
    T* Allocate()
    {
        static_assert(chunkSize > sizeof(T));

        if (freeChunkIndex == chunks.size())
        {
            AddChunk();
        }

        auto& chunk = chunks[freeChunkIndex];
        if (T* result = (T*)std::align(alignof(T), sizeof(T), chunk.p, chunk.space))
        {
            chunk.p = (uint8_t*)chunk.p + sizeof(T);
            chunk.space = chunk.space - sizeof(T);

            return result;
        }
        else
        {
            freeChunkIndex += 1;
            return Allocate<T>();
        }

        return nullptr;
    }

    void Reset()
    {
        freeChunkIndex = 0;
        for (Chunk& c : chunks)
        {
            c.space = chunkSize;
            c.p = c.data;
        }
    }

private:
    uint32_t freeChunkIndex = 0;
    std::vector<Chunk> chunks;

    // inline size_t Align(size_t size, size_t align)
    // {
    //     return (size + align - 1) & ~(align - 1);
    // }

    void AddChunk()
    {
        Chunk chunk{};
        chunk.space = chunkSize;
        chunk.data = new uint8_t[chunkSize];
        chunk.p = chunk.data;
        chunks.push_back(chunk);
    }
};
