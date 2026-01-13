#pragma once
#include <functional>

struct CmdTable
{
};

class CommandStream
{
    struct CommandHeader
    {
        std::function<void(void*)> f;
        uint32_t dataSize;
    };

public:
    template <class T>
    void Push(std::function<void(void*)>&& f, T&& data)
    {
    }

    void Execute()
    {
        auto header = reinterpret_cast<CommandHeader*>(commandBuffer.data());
        header->f(header + sizeof(CommandHeader) +);
    }

    std::vector<uint8_t> commandBuffer;
};
