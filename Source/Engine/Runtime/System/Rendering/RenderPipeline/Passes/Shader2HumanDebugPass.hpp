#pragma once
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"

namespace Rendering::Passes
{
class Shader2HumanDebugPass : public RenderPipelinePass
{
public:
    Shader2HumanDebugPass();
    ~Shader2HumanDebugPass() = default;

    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::ImageIdentifier& giS2HDebug,
        const Gfx::ImageIdentifier& hizTex,
        const Gfx::ImageIdentifier& mainColorTex,
        glm::int2 screenSize,
        const RenderingData& renderingData
    );

private:
    Shader* mergeShader;
    Material mergeMat;
};
}