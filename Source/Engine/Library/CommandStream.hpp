#pragma once
#include "MPMCQueue.hpp"
#include <cstdint>
#include <functional>
#include <new>
#include <utility>
#include <vector>

using CommandStreamFn = void (*)(void*);

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
        uint32_t dataSize;
    };

public:
    template <class T>
    void Push(CommandStreamFn f, T&& data)
    {
        size_t currentSize = commandBuffer.size();
        size_t headerAlignment = alignof(CommandHeader);
        size_t alignedStart = (currentSize + headerAlignment - 1) & ~(headerAlignment - 1);

        size_t headerSize = sizeof(CommandHeader);
        size_t dataAlignment = alignof(T);

        // Ensure data is aligned properly relative to the buffer start
        // We assume buffer start is aligned to max_align_t
        size_t minDataPos = alignedStart + headerSize;
        size_t alignedDataPos = (minDataPos + dataAlignment - 1) & ~(dataAlignment - 1);

        size_t dataOffset = alignedDataPos - alignedStart;
        size_t dataSize = sizeof(T);

        commandBuffer.resize(alignedDataPos + dataSize);

        uint8_t* ptr = commandBuffer.data() + alignedStart;
        CommandHeader* header = reinterpret_cast<CommandHeader*>(ptr);
        header->f = f;
        header->dataOffset = static_cast<uint32_t>(dataOffset);
        header->dataSize = static_cast<uint32_t>(dataSize);

        new (commandBuffer.data() + alignedDataPos) T(std::forward<T>(data));
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
            header->f(data);

            cursor += header->dataOffset + header->dataSize;
        }
        commandBuffer.clear();
    }

    std::vector<uint8_t> commandBuffer;
};
