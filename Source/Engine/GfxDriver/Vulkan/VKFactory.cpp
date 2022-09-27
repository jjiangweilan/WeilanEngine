#include "VKFactory.hpp"
#include "VKRenderPass.hpp"
#include "Exp/VKBuffer.hpp"
#include "Exp/VKShaderResource.hpp"
#include "Exp/VKShaderModule.hpp"
#include "VKShaderProgram.hpp"
#include "VKFrameBuffer.hpp"
#include "VKImage.hpp"
namespace Engine::Gfx
{
    UniPtr<Buffer> VKFactory::CreateBuffer(uint32_t size, BufferUsage usage, bool cpuVisible)
    {
        return MakeUnique<Exp::VKBuffer>(context, size, usage, cpuVisible);
    }

    UniPtr<ShaderResource> VKFactory::CreateShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency)
    {
        return MakeUnique<Exp::VKShaderResource>(context, shader, frequency);
    }

    UniPtr<RenderPass> VKFactory::CreateRenderPass()
    {
        return MakeUnique<VKRenderPass>(context);
    }

    UniPtr<FrameBuffer> VKFactory::CreateFrameBuffer(RefPtr<RenderPass> renderPass)
    {
        return MakeUnique<VKFrameBuffer>(context, renderPass);
    }

    UniPtr<Image> VKFactory::CreateImage(const ImageDescription& description, ImageUsageFlags usages)
    {
        return MakeUnique<VKImage>(context, description, usages);
    }
    UniPtr<ShaderProgram> VKFactory::CreateShaderProgram(const std::string& name, 
                                              const ShaderConfig* config,
                                              unsigned char* vert,
                                              uint32_t vertSize,
                                              unsigned char* frag,
                                              uint32_t fragSize)
    {
        return MakeUnique<VKShaderProgram>(config, context, name, vert, vertSize, frag, fragSize);
    }

}
