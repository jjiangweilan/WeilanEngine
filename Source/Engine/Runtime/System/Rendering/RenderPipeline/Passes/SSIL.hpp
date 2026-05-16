#pragma once
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include <memory>

namespace Rendering::Passes
{
class SSIL : public RenderPipelinePass
{
public:
    class GeometryPass
    {
    public:
        GeometryPass();

        void Execute(
            Gfx::CommandBuffer* cmd,
            const Gfx::ImageIdentifier& hizTex,
            const Gfx::ImageIdentifier& smoothNormalTex,
            const Gfx::ImageIdentifier& destination,
            glm::int2 halfResSize,
            glm::int2 fullResSize
        );

    private:
        Material mat;
        Shader* shader;
    };

    class BilateralFilterPass
    {
    public:
        BilateralFilterPass();

        float depthDiffSigma = 1.0f;

        void Execute(
            Gfx::CommandBuffer* cmd,
            const Gfx::ImageIdentifier& sourceTex,
            glm::int2 sourceTexSize,
            glm::int2 highResTexSize,
            const Gfx::ImageIdentifier& lowGeometryTex,
            const Gfx::ImageIdentifier& highDepth,
            const Gfx::ImageIdentifier& highSmoothNormal,
            const Gfx::ImageIdentifier& destination
        );

    private:
        Material mat;
        Shader* shader;
    };

    SSIL();
    ~SSIL() = default;

    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::ImageIdentifier& colorTex,
        const Gfx::ImageIdentifier& hizTex,
        const Gfx::ImageIdentifier& albedoTex,
        const Gfx::ImageIdentifier& motionVectorTex,
        const Gfx::ImageIdentifier& targetColor,
        RenderPipelineSetting* setting,
        RenderingData& renderingData
    );

    Gfx::ImageIdentifier& GetOutputId() { return ssil; }
    void ResetDebugState();

    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    void EnsureHistoryBuffers(int width, int height);

    Shader* ssilShader;
    Shader* smoothNormalShader;
    Shader* temporalAccumulationShader;
    Material mat;
    Material smoothNormalMat;
    Material temporalAccumulationMat;
    Gfx::ImageIdentifier ssilRaw = "SSIL_Raw";
    Gfx::ImageIdentifier ssilUpscaled = "SSIL_Upscaled";
    Gfx::ImageIdentifier ssil = "SSIL_Output";
    Gfx::ImageIdentifier ssilGeometry = "SSIL_Geometry";
    Gfx::ImageIdentifier ssilSmoothNormal = "SSIL_SmoothNormal";
    Gfx::ImageIdentifier firstFilterPassOutput = "SSIL_Filter1";
    std::unique_ptr<GeometryPass> geometryPass;
    std::unique_ptr<BilateralFilterPass> firstFilterPass;

    std::unique_ptr<Gfx::Image> historySsil;
    std::unique_ptr<Gfx::Image> historyDepth;
    std::unique_ptr<Gfx::Image> historySmoothNormal;
    glm::int2 historySize = {0, 0};

    Gfx::PipelineConfig combineConfig;

    bool debugSSIL = false;
};
} // namespace Rendering::Passes
