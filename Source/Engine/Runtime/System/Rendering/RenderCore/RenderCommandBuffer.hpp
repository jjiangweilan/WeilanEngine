#pragma once
#include "Engine/Driver/GfxDriver/GfxEnums.hpp"
#include "Engine/Library/CommandStream.hpp"
#include "Engine/Library/UUID.hpp"
#include "RenderCoreData.hpp"
#include <fmt/format.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <unordered_map>

struct AllocateTempImageData
{
    Gfx::RenderImageDescriptor desc;
    Gfx::ImageIdentifier id;
};

class CommandProcessor : public CommandStreamContext
{
public:
    Gfx::CommandBuffer* gfxCmdBuf;
};

class RenderResourceAllocator
{
public:
    Gfx::Image* GetImage(const UUID& hash)
    {
        auto iter = images.find(hash);
        if (iter != images.end())
        {
            return iter->second.image.get();
        }
        return nullptr;
    }

    Gfx::Image* Request(const Gfx::ImageIdentifier& id, const Gfx::RenderImageDescriptor& desc)
    {
        const auto& uuid = id.GetAsUUID();
        auto iter = images.find(uuid);
        if (iter != images.end() && iter->second.desc == desc)
        {
            iter->second.frameCountFromLastRequest = 0;
            return iter->second.image.get();
        }
        else
        {
            if (iter != images.end())
            {
                images.erase(iter);
            }

            Gfx::ImageDescription imageDesc;
            imageDesc.width = desc.GetWidth();
            imageDesc.height = desc.GetHeight();
            imageDesc.depth = 1;
            imageDesc.format = desc.GetFormat();
            imageDesc.multiSampling = Gfx::MultiSampling::Sample_Count_1;
            imageDesc.mipLevels = 1;
            imageDesc.isCubemap = false;

            images[uuid] = {
                GetGfxDriver()->CreateImage(
                    imageDesc,
                    (Gfx::IsColoFormat(imageDesc.format) ? Gfx::ImageUsage::ColorAttachment
                                                         : Gfx::ImageUsage::DepthStencilAttachment) |
                        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture |
                        (desc.GetRandomWrite() ? Gfx::ImageUsage::Storage : 0)
                ),
                0,
                desc
            };

            const auto& uuidStr = uuid.ToString();
            const auto& idName = id.GetName();
            auto& image = images[uuid].image;
            image->SetName(
                fmt::format(
                    "rg-{}-{}",
                    idName.empty() ? uuidStr : idName,
                    reinterpret_cast<size_t>(image.get())
                )
            );
            SPDLOG_TRACE(
                "VKCommandBufferProcessor: create new iamge({}) {}",
                reinterpret_cast<size_t>(image.get()),
                uuidStr
            );
            return image.get();
        }
    }

    void UpdateUnusedFrames()
    {
        int removeCount = 0;
        UUID readyToRemove[8];
        for (auto& iter : images)
        {
            if (iter.second.frameCountFromLastRequest > maxResourceUnusedFrames && removeCount < 8)
            {
                readyToRemove[removeCount++] = iter.first;
            }
            iter.second.frameCountFromLastRequest += 1;
        }

        for (int i = 0; i < removeCount; ++i)
        {
            images.erase(readyToRemove[i]);
        }
    }

private:
    int maxResourceUnusedFrames = 8;

    struct AllocatedImage
    {
        std::unique_ptr<Gfx::Image> image;
        int frameCountFromLastRequest = 0;
        Gfx::RenderImageDescriptor desc;
    };

    std::unordered_map<UUID, AllocatedImage> images;
};

class RenderCommandBuffer
{
public:
    RenderCommandBuffer(CommandProcessor* context, RenderResourceAllocator* resourceAllocator)
        : cm(context), resourceAllocator(resourceAllocator)
    {
    }

    void AllocateTempImage(const Gfx::RenderImageDescriptor& desc, Gfx::ImageIdentifier& id);

    void BeginLabel(std::string_view label, const float4& color);
    void EndLabel();
    void SetRenderPass(std::span<const Gfx::RenderAttachment> images);
    void SetClearValues(std::span<Gfx::ClearValue> clearValues);
    void BindResource(uint32_t set, Gfx::ShaderResource* resource);
    void BindResource(uint32_t set, const std::vector<Gfx::DynamicBinding>& bindings);
    void SetPushConstant(void* data);
    void DrawMesh(const MeshHandle& meshHandle, Gfx::ShaderProgram* shaderProgram, const Gfx::PipelineConfig& pipelineConfig);
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    void DrawIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void DrawIndexedIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void Blit(Gfx::ImageIdentifier src, Gfx::ImageIdentifier dst, Gfx::BlitOp blitOp = {});
    void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect);
    void SetViewport(const Gfx::Viewport& viewport);
    void SetLineWidth(float lineWidth);
    void SetDepthBias(float constantFactor, float clamp, float slopeFactor);
    void SetDepthBiasEnable(bool enable);
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void DispatchIndirect(Gfx::Buffer* buffer, size_t bufferOffset);

private:
    CommandStream cm;
    RenderResourceAllocator* resourceAllocator;
};

struct BeginLabelCmd
{
    const char* str;
    float4 color;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        BeginLabelCmd* cmd = (BeginLabelCmd*)ptr;
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;

        gfxCmd->BeginLabel(cmd->str, cmd->color);
    }
};

struct EndLabelCmd
{
    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        (void)ptr;

        gfxCmd->EndLabel();
    }
};

struct SetRenderPassCmd
{
    Gfx::RenderAttachment* attachments;
    uint32_t count;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        SetRenderPassCmd* cmd = (SetRenderPassCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct SetClearValuesCmd
{
    Gfx::ClearValue* values;
    uint32_t count;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        SetClearValuesCmd* cmd = (SetClearValuesCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct BindResourceCmd
{
    uint32_t set;
    Gfx::ShaderResource* resource;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        BindResourceCmd* cmd = (BindResourceCmd*)ptr;

        gfxCmd->BindResource(cmd->set, cmd->resource);
    }
};

struct BindDynamicBindingsCmd
{
    Gfx::DynamicBinding* bindings;
    uint32_t count;
    uint32_t set;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        BindDynamicBindingsCmd* cmd = (BindDynamicBindingsCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct SetPushConstantCmd
{
    void* data;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        SetPushConstantCmd* cmd = (SetPushConstantCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct DrawMeshCmd
{
    MeshHandle meshHandle;
    Gfx::ShaderProgram* shaderProgram;
    Gfx::PipelineConfig pipelineConfig;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        DrawMeshCmd* cmd = (DrawMeshCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct DrawCmd
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        DrawCmd* cmd = (DrawCmd*)ptr;
        (void)rcContext;
        (void)cmd;

        gfxCmd->Draw(cmd->vertexCount, cmd->instanceCount, cmd->firstVertex, cmd->firstInstance);
    }
};

struct DrawIndirectCmd
{
    Gfx::Buffer* buffer;
    size_t offset;
    uint32_t drawCount;
    uint32_t stride;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        DrawIndirectCmd* cmd = (DrawIndirectCmd*)ptr;

        gfxCmd->DrawIndirect(cmd->buffer, cmd->offset, cmd->drawCount, cmd->stride);
    }
};

struct DrawIndexedIndirectCmd
{
    Gfx::Buffer* buffer;
    size_t offset;
    uint32_t drawCount;
    uint32_t stride;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        DrawIndexedIndirectCmd* cmd = (DrawIndexedIndirectCmd*)ptr;

        gfxCmd->DrawIndexedIndirect(cmd->buffer, cmd->offset, cmd->drawCount, cmd->stride);
    }
};

struct BlitCmd
{
    Gfx::ImageIdentifier src;
    Gfx::ImageIdentifier dst;
    Gfx::BlitOp op;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        BlitCmd* cmd = (BlitCmd*)ptr;

        gfxCmd->Blit(cmd->src, cmd->dst, cmd->op);
    }
};

struct SetScissorCmd
{
    uint32_t firstScissor;
    uint32_t scissorCount;
    Rect2D* rects;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        SetScissorCmd* cmd = (SetScissorCmd*)ptr;

        gfxCmd->SetScissor(cmd->firstScissor, cmd->scissorCount, cmd->rects);
    }
};

struct SetViewportCmd
{
    Gfx::Viewport viewport;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        SetViewportCmd* cmd = (SetViewportCmd*)ptr;

        gfxCmd->SetViewport(cmd->viewport);
    }
};

struct SetLineWidthCmd
{
    float lineWidth;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        SetLineWidthCmd* cmd = (SetLineWidthCmd*)ptr;

        gfxCmd->SetLineWidth(cmd->lineWidth);
    }
};

struct SetDepthBiasCmd
{
    float constantFactor;
    float clamp;
    float slopeFactor;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        SetDepthBiasCmd* cmd = (SetDepthBiasCmd*)ptr;

        gfxCmd->SetDepthBias(cmd->constantFactor, cmd->clamp, cmd->slopeFactor);
    }
};

struct SetDepthBiasEnableCmd
{
    bool enable;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        SetDepthBiasEnableCmd* cmd = (SetDepthBiasEnableCmd*)ptr;

        gfxCmd->SetDepthBiasEnable(cmd->enable);
    }
};

struct DispatchCmd
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        DispatchCmd* cmd = (DispatchCmd*)ptr;

        gfxCmd->Dispatch(cmd->groupCountX, cmd->groupCountY, cmd->groupCountZ);
    }
};

struct DispatchIndirectCmd
{
    Gfx::Buffer* buffer;
    size_t bufferOffset;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        CommandProcessor* rcContext = static_cast<CommandProcessor*>(context);
        Gfx::CommandBuffer* gfxCmd = rcContext->gfxCmdBuf;
        DispatchIndirectCmd* cmd = (DispatchIndirectCmd*)ptr;

        gfxCmd->DispatchIndirect(cmd->buffer, cmd->bufferOffset);
    }
};
