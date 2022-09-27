#pragma once
#include "CommandBuffer.hpp"
#include "Core/Graphics/FrameContext.hpp"
namespace Engine
{
namespace Gfx
{
    class ShaderResource;
}
    class RenderContext
    {
        public:
            virtual ~RenderContext(){};
            virtual UniPtr<CommandBuffer> CreateCommandBuffer() = 0;
            virtual void ExecuteCommandBuffer(UniPtr<CommandBuffer>&& commandBuffer) = 0;
            
            virtual void BeginFrame(RefPtr<Gfx::ShaderResource> globalResource) = 0;
            virtual void EndFrame() = 0;
    };
}
