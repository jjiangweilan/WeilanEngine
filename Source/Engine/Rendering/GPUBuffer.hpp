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
        else
        {
            usages |= Gfx::BufferUsage::Uniform;
        }
        usages |= Gfx::BufferUsage::Transfer_Dst;

        buffer = GetGfxDriver()->CreateBuffer(sizeof(T), usages, false, gpuWrite, name);
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
