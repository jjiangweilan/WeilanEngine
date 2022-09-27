#pragma once
#include "GfxEnums.hpp"
namespace Engine::Gfx
{
    class Buffer
    {
        public:
            Buffer(uint32_t size) : size(size) {};
            virtual ~Buffer() {};
            virtual void Write(void* data, uint32_t dataSize, uint32_t offsetInDst) = 0;
            virtual void* GetCPUVisibleAddress() = 0;
            virtual void Resize(uint32_t size) = 0;

            virtual uint32_t GetSize() {return size;}

        protected:
            uint32_t size;

    };
}