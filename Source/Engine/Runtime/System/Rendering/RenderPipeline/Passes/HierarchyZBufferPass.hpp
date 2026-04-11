#pragma once
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
class HierarchyZBufferPass : public RenderPipelinePass
{
public:
    HierarchyZBufferPass();
    ~HierarchyZBufferPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& srcDepth,
        const Gfx::RenderImageDescriptor& srcDepthDesc,
        const RenderingData& renderingData
    );

    const Gfx::ImageIdentifier& GetOutputId() const { return hierarchyZBuffers[currentFrame]; }
    const Gfx::ImageIdentifier& GetHistoryOutputId() const { return hierarchyZBuffers[(currentFrame + 1) % 2]; }
    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    Shader* mip0Shader = nullptr;
    Shader* downsampleShader = nullptr;
    Material mip0Material;
    std::vector<std::unique_ptr<Material>> downsampleMaterials;
    Gfx::ImageIdentifier hierarchyZBuffers[2] = {"HierarchyZBuffer0", "HierarchyZBuffer1"};
    int currentFrame = 0;
    bool debugView = false;
};
} // namespace Rendering::Passes
