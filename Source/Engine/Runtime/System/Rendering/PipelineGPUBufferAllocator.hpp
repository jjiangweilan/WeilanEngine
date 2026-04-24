#pragma once
#include "PipelineGPUBuffer.hpp"

class PipelineGPUBufferAllocator
{
public:
    /**
     * @brief before user write to PipelineGPUBuffer for each frame, they should call this function to ensure the buffer is valid.
     *
     * this function makes sure the buffer's content doesn't change if the size requested is less than or equal to the previously allocated size.
     *
     * TODO:
     *  1. rename the buffer if it's allocated from released buffers pool
     *  2. try to reuse buffers from released buffers pool
     *
     * @param buffer the buffer to update
     * @param size requested buffer size
     */
    void AllocateBuffer(PipelineGPUBuffer& buffer, size_t size);

private:
    struct GPUBufferCached
    {
        std::unique_ptr<Gfx::Buffer> buffer;
    };

    std::unordered_map<int, GPUBufferCached> allocatedBuffers;

public:
    static PipelineGPUBuffer RequestGPUBuffer(const char* name, PipelineGPUBufferUsage usage);
    static void ReturnBuffer(PipelineGPUBuffer& buffer);

private:
    static uint64_t& GetNextBufferHandle();
    static std::vector<uint64_t>& GetReleasedBufferHandles();
};
