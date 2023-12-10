#pragma once
#include "GfxEnums.hpp"
#include <string>
namespace Engine::Gfx
{
enum class IndexBufferType
{
    UInt16,
    UInt32
};

class Buffer
{
public:
    struct CreateInfo
    {
        BufferUsageFlags usages;
        size_t size;
        bool visibleInCPU;
        const char* debugName;
    };

public:
    Buffer(BufferUsageFlags usages) : bufferUsages(usages){};
    virtual ~Buffer(){};
    virtual void* GetCPUVisibleAddress() = 0;
    virtual void SetDebugName(const char* name) = 0;
    virtual size_t GetSize() = 0;

    BufferUsageFlags GetUsages()
    {
        return bufferUsages;
    }

private:
    BufferUsageFlags bufferUsages;
};
} // namespace Engine::Gfx
