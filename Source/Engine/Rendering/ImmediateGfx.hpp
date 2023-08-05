#pragma once
#include "Core/Graphics/Material.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderGraph/Graph.hpp"

namespace Engine
{

template <class T>
concept OnetimeSubmitFunc = requires(T f, Gfx::CommandBuffer& cmd) { f(cmd); };

class ImmediateGfx
{
public:
    template <OnetimeSubmitFunc Func>
    static void OnetimeSubmit(Func f)
    {
        auto queue = GetGfxDriver()->GetQueue(QueueType::Main).Get();
        auto commandPool = GetGfxDriver()->CreateCommandPool({.queueFamilyIndex = queue->GetFamilyIndex()});
        std::unique_ptr<Gfx::CommandBuffer> cmd =
            commandPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];
        cmd->Begin();
        f(*cmd);
        cmd->End();
        Gfx::CommandBuffer* cmdBufs[] = {cmd.get()};
        GetGfxDriver()->QueueSubmit(queue, cmdBufs, {}, {}, {}, nullptr);
        GetGfxDriver()->WaitForIdle();
        cmd = nullptr;
        commandPool = nullptr;
    }

    // render the image and transfer the layout to finalLayout
    // the method will try to bindlessly render 3 vertices,
    // use should use gl_VertexIndex in vertex shader for a fullscreen mesh
    static void RenderToImage(
        Gfx::Image& image,
        Gfx::ShaderProgram& shader,
        Gfx::ShaderConfig& config,
        Gfx::ShaderResource& resource,
        Gfx::ImageLayout finalLayout
    );

    // copy a image to buffer, the image's layout should be TransferSrc, then method won't check this requirement
    static void CopyImageToBuffer(Gfx::Image& image, Gfx::Buffer& buffer);
};
} // namespace Engine
