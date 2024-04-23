#pragma once
#include "Core/SafeReferenceable.hpp"
#include "GfxEnums.hpp"
#include <string>
namespace Gfx
{
enum class IndexBufferType
{
    UInt16,
    UInt32
};

class Buffer : public SafeReferenceable<Buffer>
{
public:
    struct CreateInfo
    {
        BufferUsageFlags usages;
        size_t size;
        bool visibleInCPU;
        const char* debugName;
        bool gpuWrite;
    };

public:
    Buffer(BufferUsageFlags usages, bool gpuWrite) : bufferUsages(usages), gpuWrite(gpuWrite){};
    virtual ~Buffer(){};
    virtual void* GetCPUVisibleAddress() = 0;
    virtual void SetDebugName(const char* name) = 0;
    virtual size_t GetSize() = 0;
    bool IsGPUWrite()
    {
        return gpuWrite;
    };

    BufferUsageFlags GetUsages()
    {
        return bufferUsages;
    }

private:
    BufferUsageFlags bufferUsages;
    bool gpuWrite;
};
} // namespace Gfx
