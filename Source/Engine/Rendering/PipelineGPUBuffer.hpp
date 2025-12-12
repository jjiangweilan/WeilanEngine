#pragma once
#include "GfxDriver/Buffer.hpp"

enum class PipelineGPUBufferUsage
{
    Uniform,
    Stoage
};

class PipelineGPUBuffer
{
    friend class PipelineGPUBufferAllocator;

public:
    void Write(void* data, size_t size);
    Gfx::Buffer* GetBuffer() const { return buffer; }
    const std::string& GetName() const { return name; }
    PipelineGPUBufferUsage GetUsage() const { return usage; }

private:
    uint64_t GetHandle() { return handle; }

    PipelineGPUBufferUsage usage = PipelineGPUBufferUsage::Uniform;
    std::string name = "";
    uint64_t handle = 0;
    Gfx::Buffer* buffer = nullptr;
};
