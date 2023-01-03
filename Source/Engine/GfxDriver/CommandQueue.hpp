#pragma once
#include <cinttypes>
namespace Engine
{
enum class QueueType : uint32_t
{
    Main,
    Graphics0,
};

class CommandQueue
{
public:
    virtual uint32_t GetFamilyIndex() = 0;
    virtual ~CommandQueue(){};
};
} // namespace Engine
