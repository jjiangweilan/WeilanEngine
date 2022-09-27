#pragma once

#include "../GfxFactory.hpp"

namespace Engine::Gfx
{
    struct VKContext;
    class VKFactory : public GfxFactory
    {
    public:
        VKFactory() = default;
        VKFactory(VKFactory&& factory) noexcept : context(factory.context) {}

        UniPtr<Buffer> CreateBuffer(uint32_t size, BufferUsage usage, bool cpuVisible = false) override;
        UniPtr<ShaderResource> CreateShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency) override;
        UniPtr<RenderPass> CreateRenderPass() override;
        UniPtr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass> renderPass) override;
        UniPtr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) override;

        UniPtr<ShaderProgram> CreateShaderProgram(const std::string& name, 
                                                          const ShaderConfig* config,
                                                          unsigned char* vert,
                                                          uint32_t vertSize,
                                                          unsigned char* frag,
                                                          uint32_t fragSize) override;

    private:
        RefPtr<VKContext> context;
        friend class VKDriver;
    };
}
