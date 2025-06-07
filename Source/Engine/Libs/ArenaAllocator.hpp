#pragma once
#include <memory>
#include <vector>

// fixme: a falsely named allocator, there can be replaced by Allocator/SlabAllocator
template <size_t chunkSize>
class ArenaAllocator
{
    struct Chunk
    {
        size_t space;
        void* p; // next allocate pointer
        uint8_t* data;
    };

public:
    ArenaAllocator()
    {
        AddChunk();
        freeChunkIndex = 0;
    };

    ArenaAllocator(ArenaAllocator&& other) noexcept
        : freeChunkIndex(other.freeChunkIndex), chunks(std::move(other.chunks))
    {
        other.freeChunkIndex = 0;
    }

    // Move assignment operator
    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept
    {
        if (this != &other)
        {
            // Release current resources
            for (auto& c : chunks)
            {
                delete[] c.data;
            }

            // Transfer ownership
            freeChunkIndex = other.freeChunkIndex;
            chunks = std::move(other.chunks);

            // Reset the source object
            other.freeChunkIndex = 0;
        }
        return *this;
    }

    ~ArenaAllocator()
    {
        for (auto& c : chunks)
        {
            delete[] c.data;
        }
    }

    void* GetChuckData(int chunkIndex)
    {
        if (chunkIndex > 0 && chunkIndex < chunks.size())
        {
            return chunks[chunkIndex];
        }

        return nullptr;
    }

    template <class T>
    T* Allocate(size_t count = 1)
    {
        size_t size = sizeof(T) * count;

        if (size > chunkSize)
            return nullptr;

        if (freeChunkIndex == chunks.size())
        {
            AddChunk();
        }

        auto& chunk = chunks[freeChunkIndex];
        if (T* result = (T*)std::align(alignof(T), size, chunk.p, chunk.space))
        {
            chunk.p = (uint8_t*)chunk.p + size;
            chunk.space = chunk.space - size;

            return result;
        }
        else
        {
            freeChunkIndex += 1;
            return Allocate<T>(count);
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
