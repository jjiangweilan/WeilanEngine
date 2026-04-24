#pragma once
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
class StaticMotionVectorPass : public RenderPipelinePass
{
public:
    StaticMotionVectorPass();
    ~StaticMotionVectorPass() override = default;

    void Execute(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& depth,
        const Gfx::RenderImageDescriptor& depthDesc,
        const RenderingData& renderingData
    );

    const Gfx::ImageIdentifier& GetOutputId() const { return motionVector; }
    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    Shader* shader = nullptr;
    Material material;
    Gfx::ImageIdentifier motionVector = "StaticMotionVector";
    Gfx::RenderPass renderPass = Gfx::RenderPass(1, 1);
    bool debugView = false;
};
} // namespace Rendering::Passes
