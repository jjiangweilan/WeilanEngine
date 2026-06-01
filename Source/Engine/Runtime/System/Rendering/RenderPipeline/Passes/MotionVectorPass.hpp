#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <span>

class GrassSurface;
class GrassSurfaceRenderer;

namespace Rendering::Passes
{
class MotionVectorPass : public RenderPipelinePass
{
public:
    MotionVectorPass();
    ~MotionVectorPass() override = default;

    void Execute(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& depth,
        const Gfx::RenderImageDescriptor& depthDesc,
        const RenderingData& renderingData,
        Gfx::Buffer* indirectCommandBuffer,
        std::span<const GPUObjectShaderGroup> gpuObjectShaderGroups,
        uint32_t dynamicMotionDataOffset,
        GrassSurfaceRenderer* grassSurfaceRenderer = nullptr,
        std::span<GrassSurface*> grassSurfaces = {}
    );

    const Gfx::ImageIdentifier& GetOutputId() const { return motionVector; }
    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    void DrawStaticMotionVectors(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& depth,
        const Gfx::RenderImageDescriptor& depthDesc
    );
    void DrawDynamicAndGrassMotionVectors(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& depth,
        const RenderingData& renderingData,
        Gfx::Buffer* indirectCommandBuffer,
        std::span<const GPUObjectShaderGroup> gpuObjectShaderGroups,
        uint32_t dynamicMotionDataOffset,
        GrassSurfaceRenderer* grassSurfaceRenderer,
        std::span<GrassSurface*> grassSurfaces
    );

    Shader* staticShader = nullptr;
    Material staticMaterial;
    Gfx::RenderPass staticRenderPass = Gfx::RenderPass(1, 1);

    ObjPtr<Shader> dynamicShader;

    Gfx::ImageIdentifier motionVector = "MotionVector";
    bool debugView = false;
};
} // namespace Rendering::Passes
