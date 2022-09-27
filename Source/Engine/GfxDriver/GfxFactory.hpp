#pragma once

#include "Code/Ptr.hpp"
#include "GfxEnums.hpp"
#include "ImageDescription.hpp"
#include "Buffer.hpp"
#include "ShaderResource.hpp"
#include "ShaderProgram.hpp"
#include "RenderPass.hpp"
#include "FrameBuffer.hpp"
#include "Image.hpp"
#include "GfxEnums.hpp"
#include "ShaderModule.hpp"
#include "ShaderProgram.hpp"

namespace Engine::Gfx
{
    class GfxFactory
    {
    public:
        virtual ~GfxFactory() {};
        // readback: if cpu needs to read this buffer
        virtual UniPtr<Buffer> CreateBuffer(uint32_t size, BufferUsage usage, bool cpuVisible = false) = 0;
        virtual UniPtr<ShaderResource> CreateShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency) = 0;
        virtual UniPtr<RenderPass> CreateRenderPass() = 0;
        virtual UniPtr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass> renderPass) = 0;
        virtual UniPtr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) = 0;
        virtual UniPtr<ShaderProgram> CreateShaderProgram(const std::string& name, 
                                                          const ShaderConfig* config,
                                                          unsigned char* vert,
                                                          uint32_t vertSize,
                                                          unsigned char* frag,
                                                          uint32_t fragSize) = 0;

    public:
        static RefPtr<Gfx::GfxFactory> Instance() { return singleton; }
        static void Init(UniPtr<Gfx::GfxFactory>&& factory) { singleton = std::move(factory); }
    private:
        static UniPtr<Gfx::GfxFactory> singleton;
    };
}
