#include "PipelineGPUBufferAllocator.hpp"
#include "GfxDriver/GfxDriver.hpp"

uint64_t& PipelineGPUBufferAllocator::GetNextBufferHandle()
{
    static uint64_t handle = 0;
    handle++;
    return handle;
}

void PipelineGPUBufferAllocator::AllocateBuffer(PipelineGPUBuffer& buffer, size_t size)
{
    auto cachedIter = allocatedBuffers.find(buffer.GetHandle());
    // already allocated and large enough
    if (cachedIter != allocatedBuffers.end() && cachedIter->second.buffer->GetSize() >= size)
    {
        // give the actual buffer of this pipeline to the caller
        buffer.buffer = cachedIter->second.buffer.get();
        return;
    }

    PipelineGPUBufferUsage usage = buffer.GetUsage();
    auto newBuffer = GetGfxDriver()->CreateBuffer(size, (usage == PipelineGPUBufferUsage::Uniform ? Gfx::BufferUsage::Uniform : Gfx::BufferUsage::Storage) | Gfx::BufferUsage::Transfer_Dst, false, false, buffer.GetName().c_str());
    buffer.buffer = newBuffer.get();
    allocatedBuffers[buffer.GetHandle()] = {std::move(newBuffer)};
}

void PipelineGPUBufferAllocator::ReturnBuffer(PipelineGPUBuffer& buffer)
{
    GetReleasedBufferHandles().push_back(buffer.GetHandle());
}

PipelineGPUBuffer PipelineGPUBufferAllocator::RequestGPUBuffer(const char* name, PipelineGPUBufferUsage usage)
{
    PipelineGPUBuffer buffer;
    buffer.name = name;
    buffer.usage = usage;
    buffer.handle = GetNextBufferHandle();
    return buffer;
}

std::vector<uint64_t>& PipelineGPUBufferAllocator::GetReleasedBufferHandles()
{
    static std::vector<uint64_t> instance = {};
    return instance;
}
