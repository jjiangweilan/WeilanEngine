#pragma once
#include <cinttypes>
enum class QueueType : uint32_t
{
    Main,
};

class CommandQueue
{
public:
    virtual uint32_t GetFamilyIndex() = 0;
    virtual ~CommandQueue(){};
};
