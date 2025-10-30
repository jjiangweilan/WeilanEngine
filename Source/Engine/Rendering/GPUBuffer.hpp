#pragma once
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/GfxDriver.hpp"

template <class T>
class GPUBuffer
{
    T cpuVal;
    std::unique_ptr<Gfx::Buffer> buffer;

public:
    GPUBuffer(bool storageBuffer = false, const char* name = "GPUBuffer")
    {
        Gfx::BufferUsageFlags usages = Gfx::BufferUsage::None;
        bool gpuWrite = false;
        if (storageBuffer)
        {
            usages |= Gfx::BufferUsage::Storage;
            gpuWrite = true;
        }

        buffer = GetGfxDriver()->CreateBuffer(sizeof(T), Gfx::BufferUsage::Storage, false, gpuWrite, name);
    }

    size_t GetSize()
    {
        return sizeof(T);
    }

    T* GetPtr()
    {
        return &cpuVal;
    }

    T* operator->()
    {
        return &cpuVal;
    }

    Gfx::Buffer* operator*()
    {
        return buffer.get();
    }
};
