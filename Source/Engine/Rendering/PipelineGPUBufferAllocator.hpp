#pragma once
#include "GfxDriver/Buffer.hpp"

enum class PipelineGPUBufferUsage
{
    Uniform,
    Stoage
};

class PipelineGPUBuffer
{
public:
private:
    bool valid = false;
    PipelineGPUBufferUsage usage;
};

class PipelineGPUBufferAllocator
{
    /**
     * @brief before user write to PipelineGPUBuffer for each frame, they should call this function to ensure the buffer is valid.
     *
     * @param buffer the buffer to update
     * @param size requested buffer size
     */
    void AllocateBuffer(PipelineGPUBuffer& buffer, size_t size);

    static PipelineGPUBuffer RequestGPUBuffer();

private:
    static int nextBufferHandle;
};
