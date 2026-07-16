#pragma once

#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"

namespace Rendering::Passes
{
class TAAPass : public RenderPipelinePass
{
public:
    TAAPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& currentColor,
        const Gfx::ImageIdentifier& currentDepth,
        const Gfx::ImageIdentifier& motionVectors,
        const Gfx::RenderImageDescriptor& colorDesc,
        const RenderPipelineSetting::TAA& settings,
        const RenderingData& renderingData
    );

    void ResetHistory();
    const Gfx::ImageIdentifier& GetOutputId() const { return output; }

    static glm::vec2 GetProjectionJitterNdc(
        uint32_t frameIndex,
        const glm::vec2& renderSize,
        float jitterScale
    );

private:
    void EnsureHistoryBuffers(int width, int height);

    ObjPtr<Shader> resolveShader;
    ObjPtr<Shader> sharpenShader;
    Material resolveMaterial;
    Material sharpenMaterial;
    Gfx::ImageIdentifier output = "TAA_Output";
    Gfx::ImageIdentifier historyOutput = "TAA_HistoryOutput";
    Gfx::ImageIdentifier historyDepthOutput = "TAA_HistoryDepthOutput";
    std::unique_ptr<Gfx::Image> historyColor;
    std::unique_ptr<Gfx::Image> historyDepth;
    glm::int2 historySize = {0, 0};
    Scene* historyScene = nullptr;
    Camera* historyCamera = nullptr;
    bool historyValid = false;
};
} // namespace Rendering::Passes
