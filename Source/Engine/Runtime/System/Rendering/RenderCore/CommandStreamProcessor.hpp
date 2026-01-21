#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/CommandStream.hpp"
#include "RenderCoreData.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

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

class CommandStreamProcessor : public CommandStreamContext
{
public:
    static void BeginLabel(CommandStreamContext* selfPtr, void* cmdData);
    static void EndLabel(CommandStreamContext* selfPtr, void* cmdData);
    static void SetRenderPass(CommandStreamContext* selfPtr, void* cmdData);
    static void SetClearValues(CommandStreamContext* selfPtr, void* cmdData);
    static void BindResource(CommandStreamContext* selfPtr, void* cmdData);
    static void BindDynamicBindings(CommandStreamContext* selfPtr, void* cmdData);
    static void SetPushConstant(CommandStreamContext* selfPtr, void* cmdData);
    static void DrawMesh(CommandStreamContext* selfPtr, void* cmdData);
    static void Draw(CommandStreamContext* selfPtr, void* cmdData);
    static void DrawIndirect(CommandStreamContext* selfPtr, void* cmdData);
    static void DrawIndexedIndirect(CommandStreamContext* selfPtr, void* cmdData);
    static void Blit(CommandStreamContext* selfPtr, void* cmdData);
    static void SetScissor(CommandStreamContext* selfPtr, void* cmdData);
    static void SetViewport(CommandStreamContext* selfPtr, void* cmdData);
    static void SetLineWidth(CommandStreamContext* selfPtr, void* cmdData);
    static void SetDepthBias(CommandStreamContext* selfPtr, void* cmdData);
    static void SetDepthBiasEnable(CommandStreamContext* selfPtr, void* cmdData);
    static void Dispatch(CommandStreamContext* selfPtr, void* cmdData);
    static void DispatchIndirect(CommandStreamContext* selfPtr, void* cmdData);

private:
    Gfx::CommandBuffer* gfxCmdBuf;
};
