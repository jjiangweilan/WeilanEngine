#include "PipelineGPUBuffer.hpp"
#include "Driver/GfxDriver/GfxDriver.hpp"

void PipelineGPUBuffer::Write(void* data, size_t size)
{
    GetGfxDriver()->UploadBuffer(
        *buffer,
        (uint8_t*)data,
        size
    );
}
