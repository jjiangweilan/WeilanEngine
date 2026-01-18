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
};

struct EndLabelCmd
{
};

struct SetRenderPassCmd
{
    Gfx::RenderAttachment* attachments;
    uint32_t count;
};

struct SetClearValuesCmd
{
    Gfx::ClearValue* values;
    uint32_t count;
};

struct BindResourceCmd
{
    uint32_t set;
    Gfx::ShaderResource* resource;
};

struct BindDynamicBindingsCmd
{
    Gfx::DynamicBinding* bindings;
    uint32_t count;
    uint32_t set;
};

struct SetPushConstantCmd
{
    void* data;
};

struct DrawMeshCmd
{
    MeshHandle meshHandle;
    Gfx::ShaderProgram* shaderProgram;
    Gfx::PipelineConfig pipelineConfig;
};

struct DrawCmd
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

struct DrawIndirectCmd
{
    Gfx::Buffer* buffer;
    size_t offset;
    uint32_t drawCount;
    uint32_t stride;
};

struct DrawIndexedIndirectCmd
{
    Gfx::Buffer* buffer;
    size_t offset;
    uint32_t drawCount;
    uint32_t stride;
};

struct BlitCmd
{
    Gfx::ImageIdentifier src;
    Gfx::ImageIdentifier dst;
    Gfx::BlitOp op;
};

struct SetScissorCmd
{
    uint32_t firstScissor;
    uint32_t scissorCount;
    Rect2D* rects;
};

struct SetViewportCmd
{
    Gfx::Viewport viewport;
};

struct SetLineWidthCmd
{
    float lineWidth;
};

struct SetDepthBiasCmd
{
    float constantFactor;
    float clamp;
    float slopeFactor;
};

struct SetDepthBiasEnableCmd
{
    bool enable;
};

struct DispatchCmd
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
};

struct DispatchIndirectCmd
{
    Gfx::Buffer* buffer;
    size_t bufferOffset;
};

class CommandProcessor : public CommandStreamContext
{
public:
    Gfx::CommandBuffer* gfxCmdBuf;

    static void BeginLabel(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        BeginLabelCmd* cmd = (BeginLabelCmd*)cmdData;
        self->gfxCmdBuf->BeginLabel(cmd->str, cmd->color);
    }

    static void EndLabel(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        (void)cmdData;
        self->gfxCmdBuf->EndLabel();
    }

    static void SetRenderPass(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        SetRenderPassCmd* cmd = (SetRenderPassCmd*)cmdData;
        (void)self;
        (void)cmd;
    }

    static void SetClearValues(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        SetClearValuesCmd* cmd = (SetClearValuesCmd*)cmdData;
        (void)self;
        (void)cmd;
    }

    static void BindResource(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        BindResourceCmd* cmd = (BindResourceCmd*)cmdData;
        self->gfxCmdBuf->BindResource(cmd->set, cmd->resource);
    }

    static void BindDynamicBindings(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        BindDynamicBindingsCmd* cmd = (BindDynamicBindingsCmd*)cmdData;
        (void)self;
        (void)cmd;
    }

    static void SetPushConstant(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        SetPushConstantCmd* cmd = (SetPushConstantCmd*)cmdData;
        (void)self;
        (void)cmd;
    }

    static void DrawMesh(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        DrawMeshCmd* cmd = (DrawMeshCmd*)cmdData;
        (void)self;
        (void)cmd;
    }

    static void Draw(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        DrawCmd* cmd = (DrawCmd*)cmdData;
        self->gfxCmdBuf->Draw(cmd->vertexCount, cmd->instanceCount, cmd->firstVertex, cmd->firstInstance);
    }

    static void DrawIndirect(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        DrawIndirectCmd* cmd = (DrawIndirectCmd*)cmdData;
        self->gfxCmdBuf->DrawIndirect(cmd->buffer, cmd->offset, cmd->drawCount, cmd->stride);
    }

    static void DrawIndexedIndirect(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        DrawIndexedIndirectCmd* cmd = (DrawIndexedIndirectCmd*)cmdData;
        self->gfxCmdBuf->DrawIndexedIndirect(cmd->buffer, cmd->offset, cmd->drawCount, cmd->stride);
    }

    static void Blit(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        BlitCmd* cmd = (BlitCmd*)cmdData;
        self->gfxCmdBuf->Blit(cmd->src, cmd->dst, cmd->op);
    }

    static void SetScissor(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        SetScissorCmd* cmd = (SetScissorCmd*)cmdData;
        self->gfxCmdBuf->SetScissor(cmd->firstScissor, cmd->scissorCount, cmd->rects);
    }

    static void SetViewport(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        SetViewportCmd* cmd = (SetViewportCmd*)cmdData;
        self->gfxCmdBuf->SetViewport(cmd->viewport);
    }

    static void SetLineWidth(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        SetLineWidthCmd* cmd = (SetLineWidthCmd*)cmdData;
        self->gfxCmdBuf->SetLineWidth(cmd->lineWidth);
    }

    static void SetDepthBias(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        SetDepthBiasCmd* cmd = (SetDepthBiasCmd*)cmdData;
        self->gfxCmdBuf->SetDepthBias(cmd->constantFactor, cmd->clamp, cmd->slopeFactor);
    }

    static void SetDepthBiasEnable(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        SetDepthBiasEnableCmd* cmd = (SetDepthBiasEnableCmd*)cmdData;
        self->gfxCmdBuf->SetDepthBiasEnable(cmd->enable);
    }

    static void Dispatch(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        DispatchCmd* cmd = (DispatchCmd*)cmdData;
        self->gfxCmdBuf->Dispatch(cmd->groupCountX, cmd->groupCountY, cmd->groupCountZ);
    }

    static void DispatchIndirect(CommandStreamContext* selfPtr, void* cmdData)
    {
        CommandProcessor* self = static_cast<CommandProcessor*>(selfPtr);
        DispatchIndirectCmd* cmd = (DispatchIndirectCmd*)cmdData;
        self->gfxCmdBuf->DispatchIndirect(cmd->buffer, cmd->bufferOffset);
    }
};
