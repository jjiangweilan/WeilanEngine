#pragma once

#include "CommandBuffer.hpp"
#include "Image.hpp"
#include "GfxBuffer.hpp"
#include "Code/Ptr.hpp"

#include <vector>
#include <memory>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>

namespace Engine::Gfx
{
    enum class Backend
    {
        Vulkan, OpenGL
    };

    class GfxDriver
    {
        public:
            static RefPtr<GfxDriver> Instance() { return gfxDriver; }
            static void CreateGfxDriver(Backend backend);
            static void DestroyGfxDriver() { gfxDriver = nullptr; }

            virtual ~GfxDriver(){};

            virtual Backend GetGfxBackendType() = 0;
            virtual Extent2D GetWindowSize() = 0;
            virtual void ForceSyncResources() = 0;
            virtual void DispatchGPUWork() = 0;
            virtual SDL_Window* GetSDLWindow() = 0;
            virtual void ExecuteCommandBuffer(UniPtr<CommandBuffer>&& cmdBuf) = 0;
            virtual RefPtr<Image> GetSwapChainImageProxy() = 0;
            virtual UniPtr<GfxBuffer> CreateBuffer(uint32_t size, BufferUsage usage, bool cpuVisible = false) = 0;
            virtual UniPtr<CommandBuffer> CreateCommandBuffer() = 0;
            virtual UniPtr<ShaderResource> CreateShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency) = 0;
            virtual UniPtr<RenderPass> CreateRenderPass() = 0;
            virtual UniPtr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass> renderPass) = 0;
            virtual UniPtr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) = 0;
            virtual UniPtr<ShaderProgram> CreateShaderProgram(
                    const std::string& name, 
                    const ShaderConfig* config,
                    unsigned char* vert,
                    uint32_t vertSize,
                    unsigned char* frag,
                    uint32_t fragSize) = 0;

        private:
            static UniPtr<GfxDriver> gfxDriver;
    };
}
