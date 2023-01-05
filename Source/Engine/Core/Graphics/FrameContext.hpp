#pragma once

#include <string>
#include "Libs/Ptr.hpp"
namespace Engine
{
namespace Gfx
{
    class ShaderResource;
}
    class FrameContext
    {
        public:
            virtual ~FrameContext(){}
            virtual RefPtr<Gfx::ShaderResource> GetShaderResource();
        private:
    };
}