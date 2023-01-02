#pragma once
#include "GfxEnums.hpp"
#include <string>
namespace Engine::Gfx
{
class Buffer
{
public:
    struct CreateInfo
    {
        BufferUsageFlags usages;
        uint32_t size;
        bool visibleInCPU = false;
        const char* debugName = nullptr;
    };

public:
    virtual ~Buffer(){};
    virtual void* GetCPUVisibleAddress() = 0;
    virtual void SetDebugName(const char* name) = 0;
    virtual uint32_t GetSize() = 0;
};
} // namespace Engine::Gfx
