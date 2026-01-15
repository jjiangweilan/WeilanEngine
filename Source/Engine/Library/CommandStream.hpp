#pragma once
#include "Engine/Library/Math.hpp"
#include "MPMCQueue.hpp"
#include <cstdint>
#include <functional>
#include <new>
#include <utility>
#include <vector>

class CommandStreamContext
{
public:
    virtual ~CommandStreamContext() = default;
};

using CommandStreamFn = void (*)(CommandStreamContext* context, void*);

class CommandStream
{
    struct CommandHeader
    {
        CommandStreamFn f;
        /**
         * @brief offset from command header to data
         */
        uint32_t dataOffset;

        /**
         * @brief size of the data
         */
        uint32_t fullDataSize;
    };

public:
    CommandStream(CommandStreamContext* context) : context(context)
    {
    }

    template <class T>
    T* Push(CommandStreamFn f, T&& data, void** extraDataPtr = nullptr, size_t extraDataSize = 0)
    {
        size_t currentSize = commandBuffer.size();
        size_t headerAlignment = alignof(CommandHeader);
        size_t alignedStart = Math::AlignMemory(currentSize, headerAlignment);

        size_t headerSize = sizeof(CommandHeader);
        size_t dataAlignment = alignof(T);

        // Ensure data is aligned properly relative to the buffer start
        // We assume buffer start is aligned to max_align_t
        size_t minDataPos = alignedStart + headerSize;
        size_t alignedDataPos = Math::AlignMemory(minDataPos, dataAlignment);

        size_t dataOffset = alignedDataPos - alignedStart;
        size_t dataSize = sizeof(T);

        commandBuffer.resize(alignedDataPos + dataSize + extraDataSize);

        uint8_t* ptr = commandBuffer.data() + alignedStart;
        CommandHeader* header = reinterpret_cast<CommandHeader*>(ptr);
        header->f = f;
        header->dataOffset = static_cast<uint32_t>(dataOffset);
        header->fullDataSize = static_cast<uint32_t>(dataSize + extraDataSize);

        if (extraDataPtr && extraDataSize > 0)
        {
            *extraDataPtr = commandBuffer.data() + alignedDataPos + dataSize;
        }

        T* ret = new (commandBuffer.data() + alignedDataPos) T(std::forward<T>(data));
        return ret;
    }

    void Execute()
    {
        size_t cursor = 0;
        size_t alignment = alignof(CommandHeader);

        while (cursor < commandBuffer.size())
        {
            cursor = (cursor + alignment - 1) & ~(alignment - 1);
            if (cursor >= commandBuffer.size())
                break;

            uint8_t* ptr = commandBuffer.data() + cursor;
            CommandHeader* header = reinterpret_cast<CommandHeader*>(ptr);

            void* data = ptr + header->dataOffset;
            header->f(context, data);

            cursor += header->dataOffset + header->fullDataSize;
        }
        commandBuffer.clear();
    }

    CommandStreamContext* context;
    std::vector<uint8_t> commandBuffer;
};
