#pragma once
#include "Core/SafeReferenceable.hpp"
#include "GfxDriver/VertexAttributes.hpp"
#include "GfxEnums.hpp"
#include "Libs/Assert.hpp"
#include "Libs/UUID.hpp"
#include <unordered_map>
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
        BufferUsageFlags usages = BufferUsage::None;
        size_t size = 0;
        bool visibleInCPU = false;
        const char* debugName = nullptr;
        bool gpuWrite = false;
    };

    Buffer(BufferUsageFlags usages, bool gpuWrite) : bufferUsages(usages), gpuWrite(gpuWrite), uuid() {};
    virtual ~Buffer() {};
    virtual void* GetCPUVisibleAddress() = 0;
    virtual void SetDebugName(const char* name) = 0;
    virtual size_t GetSize() = 0;
    bool IsGPUWrite() { return gpuWrite; };
    BufferUsageFlags GetUsages() { return bufferUsages; }
    const UUID& GetUUID() { return uuid; }

    void SetVertexAttributes(int binding, const VertexAttributes& attributes)
    {
        this->attributes[binding] = attributes;
    }
    auto GetVertexAttributes(int binding) -> const VertexAttributes&
    {
        auto iter = attributes.find(binding);
        ASSERT(iter != attributes.end());

#if ENGINE_DEV_BUILD
        ASSERT(iter->second.GetDescription().size() != 0 && ((int)(bufferUsages & BufferUsage::Vertex) != 0));
#endif
        return iter->second;
    }

private:
    BufferUsageFlags bufferUsages = BufferUsage::None;
    std::unordered_map<int, VertexAttributes> attributes = {
    }; // describing vertex attributes when buffer is used as vertex buffer
    bool gpuWrite;
    UUID uuid;
};
} // namespace Gfx
