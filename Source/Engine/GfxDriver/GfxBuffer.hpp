#pragma once
#include "GfxEnums.hpp"
#include <string>
namespace Engine::Gfx
{
    class GfxBuffer
    {
        public:
            GfxBuffer(uint32_t size) : size(size) {};
            virtual ~GfxBuffer() {};
            virtual void Write(void* data, uint32_t dataSize, uint32_t offsetInDst) = 0;
            virtual void* GetCPUVisibleAddress() = 0;
            virtual void Resize(uint32_t size) = 0;
            virtual void SetDebugName(const std::string& name) {};

            virtual uint32_t GetSize() {return size;}

        protected:
            uint32_t size;

    };
}
