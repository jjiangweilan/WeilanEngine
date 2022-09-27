#pragma once

#include "Core/Graphics/RenderTarget.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "Core/Graphics/Material.hpp"
#include "Core/Graphics/Shader.hpp"
#include "Core/Graphics/RenderContext.hpp"

#include <vector>
#include <memory>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include "Code/Ptr.hpp"

namespace Engine
{
    enum class GfxBackend
    {
        Vulkan, OpenGL
    };
}
namespace Engine::Gfx
{
    class Image;
    class GfxDriver
    {
        public:
            virtual ~GfxDriver(){};
            void Init() 
            {
                InitGfxFactory();
            };

            virtual std::unique_ptr<RenderTarget> CreateRenderTarget(const RenderTargetDescription& renderTargetDescription) = 0;
            virtual RenderContext* GetRenderContext() = 0;
            virtual void SetPresentImage(RefPtr<Image> presentImage) = 0;
            virtual GfxBackend GetGfxBackendType() = 0;
            virtual Extent2D GetWindowSize() = 0;
            virtual void ForceSyncResources() = 0;
            virtual void DispatchGPUWork() = 0;
            virtual SDL_Window* GetSDLWindow() = 0;

        protected:
            virtual void InitGfxFactory() = 0;
    };
}
