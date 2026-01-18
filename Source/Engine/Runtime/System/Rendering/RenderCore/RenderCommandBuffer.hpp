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
    RenderImageDescriptor desc;
    ImageIdentifier id;
};

class RenderCommandBufferCommandStreamContext : public CommandStreamContext
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

    Gfx::Image* Request(const ImageIdentifier& id, const RenderImageDescriptor& desc)
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
        RenderImageDescriptor desc;
    };

    std::unordered_map<UUID, AllocatedImage> images;
};

class RenderCommandBuffer
{
public:
    RenderCommandBuffer(RenderCommandBufferCommandStreamContext* context, RenderResourceAllocator* resourceAllocator)
        : cm(context), resourceAllocator(resourceAllocator)
    {
    }

    void AllocateTempImage(const RenderImageDescriptor& desc, ImageIdentifier& id);

    void BeginLabel(std::string_view label);
    void EndLabel();
    void SetRenderPass(std::span<const RenderAttachment> images);
    void SetClearValues(std::span<Gfx::ClearValue> clearValues);
    void BindResource(uint32_t set, Gfx::ShaderResource* resource);
    void BindResource(uint32_t set, const std::vector<DynamicBinding>& bindings);
    void SetPushConstant(void* data);
    void DrawMesh(const MeshHandle& meshHandle, Gfx::ShaderProgram* shaderProgram, const Gfx::PipelineConfig& pipelineConfig);
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    void DrawIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void DrawIndexedIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride);
    void Blit(ImageIdentifier src, ImageIdentifier dst, BlitOp blitOp = {});
    void SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect);
    void SetViewport(const Viewport& viewport);
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

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        BeginLabelCmd* cmd = (BeginLabelCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct EndLabelCmd
{
    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        (void)rcContext;
        (void)ptr;
    }
};

struct SetRenderPassCmd
{
    RenderAttachment* attachments;
    uint32_t count;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
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
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
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
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        BindResourceCmd* cmd = (BindResourceCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct BindDynamicBindingsCmd
{
    DynamicBinding* bindings;
    uint32_t count;
    uint32_t set;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
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
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
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
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
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
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        DrawCmd* cmd = (DrawCmd*)ptr;
        (void)rcContext;
        (void)cmd;
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
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        DrawIndirectCmd* cmd = (DrawIndirectCmd*)ptr;
        (void)rcContext;
        (void)cmd;
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
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        DrawIndexedIndirectCmd* cmd = (DrawIndexedIndirectCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct BlitCmd
{
    ImageIdentifier src;
    ImageIdentifier dst;
    BlitOp op;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        BlitCmd* cmd = (BlitCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct SetScissorCmd
{
    uint32_t firstScissor;
    uint32_t scissorCount;
    Rect2D* rects;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        SetScissorCmd* cmd = (SetScissorCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct SetViewportCmd
{
    Viewport viewport;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        SetViewportCmd* cmd = (SetViewportCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct SetLineWidthCmd
{
    float lineWidth;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        SetLineWidthCmd* cmd = (SetLineWidthCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct SetDepthBiasCmd
{
    float constantFactor;
    float clamp;
    float slopeFactor;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        SetDepthBiasCmd* cmd = (SetDepthBiasCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct SetDepthBiasEnableCmd
{
    bool enable;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        SetDepthBiasEnableCmd* cmd = (SetDepthBiasEnableCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct DispatchCmd
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        DispatchCmd* cmd = (DispatchCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};

struct DispatchIndirectCmd
{
    Gfx::Buffer* buffer;
    size_t bufferOffset;

    static void Execute(CommandStreamContext* context, void* ptr)
    {
        RenderCommandBufferCommandStreamContext* rcContext = static_cast<RenderCommandBufferCommandStreamContext*>(context);
        DispatchIndirectCmd* cmd = (DispatchIndirectCmd*)ptr;
        (void)rcContext;
        (void)cmd;
    }
};
